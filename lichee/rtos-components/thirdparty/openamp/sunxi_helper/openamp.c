#include <hal_osal.h>
#include <openamp/sunxi_helper/openamp.h>
#include <hal_msgbox.h>
#include <hal_time.h>
#ifdef CONFIG_ARCH_RISCV_PMP
#include <pmp.h>
#endif
#include <hal_event.h>
#include <aw_list.h>
#include <sunxi_rsc_tab.h>
#ifdef CONFIG_AMP_SHARE_IRQ
#include <openamp/openamp_share_irq.h>
#endif

/*
 * openamp components init status:
 *     0: idle
 *     1: init
 *     2: ready
 */
enum openamp_stats {
	IDLE = 0,
	INIT,
	READY,
};

static enum openamp_stats init_stat[NR_RPROC] = { IDLE };
static hal_event_t lock = NULL;

static int openamp_lock(uint32_t rproc_id)
{
	int ret;

	if (rproc_id > NR_RPROC) {
		openamp_err("invalid rproc_id = %d\r\n", rproc_id);
		return -EINVAL;
	}

	if (lock == NULL) {
		lock = hal_event_create();
		hal_event_set_bits(lock, (1 << NR_RPROC) - 1);
	}

__wait:
	ret = hal_ev_wait_any(lock, 1 << rproc_id, HAL_WAIT_FOREVER);
	if (ret < 0) {
		openamp_err("wait event failed, try again.\r\n");
		goto __wait;
	}

	return 0;
}

static int openamp_unlock(uint32_t rproc_id)
{
	if (rproc_id > NR_RPROC) {
		openamp_err("invalid rproc_id = %d\r\n", rproc_id);
		return -EINVAL;
	}

	hal_event_set_bits(lock, 1 << rproc_id);

	return 0;
}

extern void master_rpmsg_ns_bind_cb(struct rpmsg_device *rdev, const char *name, uint32_t dest);

#ifdef CONFIG_AMP_SHARE_IRQ
static int amp_share_irq_init(struct remoteproc *rproc)
{
	int ret;
	struct fw_rsc_carveout *carveout;
	struct metal_io_region *shm_io;
	void *share_buf;

	carveout = resource_table_get_share_irq_entry(rproc, RPROC_IRQ_TAB);
	if (!carveout) {
		openamp_err("Can't Find shmem,id=%d\n", RPROC_IRQ_TAB);
		return -1;
	}

	shm_io = remoteproc_get_io_with_pa(rproc, carveout->pa);
	if (!shm_io) {
		openamp_err("Failed to get shared memory I/O region\n");
		return -2;
	}

	share_buf = metal_io_phys_to_virt(shm_io, carveout->pa);
	if (!share_buf) {
		openamp_err("Failed to get address of shared memory buffer\n");
		return -3;
	}

	ret = openamp_share_irq_init(share_buf, carveout->len);
	if (ret < 0) {
		openamp_err("Init Share Interrupt Table Failed,ret=%d.\n", ret);
		return -4;
	}

	return 0;
}
#endif

static int openamp_platform_init(int rproc_id, int ops_id, int rsc_id, int role)
{
	int ret;
	struct remoteproc *rproc;
	struct rpmsg_device *rpmsg_dev;
	rpmsg_ns_bind_cb bind;


	openamp_info("rproc%d init\n", rproc_id);

	if (rproc_id > NR_RPROC || rproc_id < 0) {
		openamp_err("Invalid rproc_id = %d\r\n", rproc_id);
		return -EINVAL;
	}

	openamp_lock(rproc_id);

	if (init_stat[rproc_id] == READY) {
		openamp_info("rproc%d already init.\r\n", rproc_id);
		openamp_unlock(rproc_id);
		return 0;
	}

	init_stat[rproc_id] = INIT;

	rproc = openamp_sunxi_create_rproc(rproc_id, ops_id, rsc_id);
	if (!rproc) {
		openamp_err("rproc%d: Failed to create remoteproc\n", rproc_id);
		ret = -1;
		goto out;
	}

	if (role == VIRTIO_DEV_MASTER)
		bind = master_rpmsg_ns_bind_cb;
	else
		bind = NULL;

	rpmsg_dev = openamp_sunxi_create_rpmsg_vdev(rproc, RPMSG_VDEV_ID, role, NULL, bind);
	if (!rpmsg_dev) {
		openamp_err("rproc%d: Failed to create rpmsg virtio device\n", rproc_id);
		ret = -2;
		goto release_rproc;
	}

#ifdef CONFIG_AMP_SHARE_IRQ
	ret = amp_share_irq_init(rproc);
	if (ret)
		openamp_err("Init AMP share interrupt failed, ret: %d\n", ret);
#endif

	init_stat[rproc_id] = READY;
	openamp_unlock(rproc_id);

	openamp_info("rproc%d init done\n", rproc_id);

#if defined(CONFIG_ARCH_RISCV_PMP) && !defined(CONFIG_PMP_EARLY_ENABLE)
	pmp_enable();
#endif

	return 0;

release_rproc:
	openamp_sunxi_release_rproc(rproc);
out:
	openamp_unlock(rproc_id);
	return ret;
}

static void openamp_platform_deinit(int rproc_id)
{
	struct remoteproc *rproc = openamp_sunxi_get_rproc(rproc_id);
	struct rpmsg_device *rpmsg_dev = openamp_sunxi_get_rpmsg_vdev(rproc_id);

	if (!rproc || !rpmsg_dev) {
		openamp_err("Invalid rproc_id = %d\r\n", rproc_id);
		return;
	}

	openamp_lock(rproc_id);

	openamp_sunxi_release_rpmsg_vdev(rproc, rpmsg_dev);
	openamp_sunxi_release_rproc(rproc);
	init_stat[rproc_id] = IDLE;

	openamp_unlock(rproc_id);
}

int openamp_tocpu_init(void)
{
	return openamp_platform_init(RPROC_CPU_ID, RPROC_CPU_OPS_ID, RSC_CPU_ID, VIRTIO_DEV_SLAVE);
}

void openamp_tocpu_deinit(void)
{
	openamp_platform_deinit(RPROC_CPU_ID);
}

#ifdef CONFIG_MULTI_OPENAMP_SUPPORT_DSP
int openamp_todsp_init(void)
{
#ifdef CONFIG_OPENAMP_TODSP_RPROC_MASTER
	return openamp_platform_init(RPROC_DSP_ID, RPROC_DSP_OPS_ID, RSC_DSP_ID, VIRTIO_DEV_MASTER);
#else
	return openamp_platform_init(RPROC_DSP_ID, RPROC_DSP_OPS_ID, RSC_DSP_ID, VIRTIO_DEV_SLAVE);
#endif
}

void openamp_todsp_deinit(void)
{
	openamp_platform_deinit(RPROC_DSP_ID);
}
#endif

#ifdef CONFIG_MULTI_OPENAMP_SUPPORT_RV
int openamp_torv_init(void)
{
#ifdef CONFIG_OPENAMP_TORV_RPROC_MASTER
	return openamp_platform_init(RPROC_RV_ID, RPROC_RV_OPS_ID, RSC_RV_ID, VIRTIO_DEV_MASTER);
#else
	return openamp_platform_init(RPROC_RV_ID, RPROC_RV_OPS_ID, RSC_RV_ID, VIRTIO_DEV_SLAVE);
#endif
}

void openamp_torv_deinit(void)
{
	openamp_platform_deinit(RPROC_RV_ID);
}
#endif

int openamp_init(void)
{
	int ret;

	openamp_dbg("init tocpu rproc.\r\n");
	ret = openamp_tocpu_init();
	if (ret < 0)
		openamp_err("init tocpu rproc failed.\r\n");

#ifdef CONFIG_MULTI_OPENAMP_SUPPORT_DSP
	openamp_dbg("init todsp rproc.\r\n");
	ret = openamp_todsp_init();
	if (ret < 0)
		openamp_err("init todsp rproc failed.\r\n");
#endif
#ifdef CONFIG_MULTI_OPENAMP_SUPPORT_RV
	openamp_dbg("init torv rproc.\r\n");
	ret = openamp_torv_init();
	if (ret < 0)
		openamp_err("init torv rproc failed.\r\n");
#endif

	return 0;
}

void openamp_deinit(void)
{
	openamp_tocpu_deinit();
#ifdef CONFIG_MULTI_OPENAMP_SUPPORT_DSP
	openamp_todsp_deinit();
#endif
#ifdef CONFIG_MULTI_OPENAMP_SUPPORT_RV
	openamp_torv_deinit();
#endif
}

static void openamp_init_thread(void *arg)
{
	openamp_init();
	hal_thread_stop(NULL);
}

int openamp_init_async(void)
{
	void *thread;

	thread = hal_thread_create(openamp_init_thread, NULL,
					"openamp_init", 2 * 1024, 8);
	if (thread)
		hal_thread_start(thread);

	return 0;
}

struct rpmsg_endpoint *openamp_ept_open(const char *name, int rpmsg_id,
				uint32_t src_addr, uint32_t dst_addr,void *priv,
				rpmsg_ept_cb cb, rpmsg_ns_unbind_cb unbind_cb)
{
	int ret;
	struct rpmsg_endpoint *ept = NULL;
	struct rpmsg_device *rpmsg_dev;

	if (init_stat[rpmsg_id] != READY) {
		openamp_err("Please call openamp_init to init openamp\r\n");
		return NULL;
	}

	rpmsg_dev = openamp_sunxi_get_rpmsg_vdev(rpmsg_id);
	if(!rpmsg_dev) {
		openamp_info("Can't find rpmsg device by id(%d)", rpmsg_id);
		goto out;
	}

	ept = hal_malloc(sizeof(*ept));
	if(!ept) {
		openamp_err("Failed to alloc %s rpmsg endpoint\n", name);
		goto out;
	}

	ret = rpmsg_create_ept(ept, rpmsg_dev, name,
			src_addr, dst_addr, cb, unbind_cb);

	if (ret != 0) {
		openamp_err("Failed to create rpmsg endpoint name=%s (ret: %d)\n", name, ret);
		goto free_ept;
	}

	ept->priv = priv;
	openamp_info("Waiting for ept('%s') ready\r\n", ept->name);
	while (!is_rpmsg_ept_ready(ept)) {
		hal_msleep(1);
	}
	openamp_info("ept('%s') ready! src: 0x%x, dst: 0x%x\r\n", ept->name,
					ept->addr, ept->dest_addr);

	return ept;
free_ept:
	hal_free(ept);
out:
	return NULL;
}

void openamp_ept_close(struct rpmsg_endpoint *ept)
{
	if(!ept) {
		openamp_err("Invalid arguent(ept=null)\r\n");
		return;
	}
	rpmsg_destroy_ept(ept);
	hal_free(ept);
}

int openamp_rpmsg_send(struct rpmsg_endpoint *ept, void *data, uint32_t len)
{
	int ret;
	if(ept == NULL) {
		openamp_err("Invalid endpoint arg(null)\r\n");
		return -1;
	}

	ret = rpmsg_send(ept, data, len);
	if (ret < 0)
		openamp_err("Failed to send data\n");
	return ret;
}
