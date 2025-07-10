#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <aw_list.h>
#include <hal_mutex.h>
#include <hal_mem.h>
#include <hal_cache.h>
#include <time.h>
#include <console.h>
/*head-file of cedarc*/
#include "vdecoder.h"
#include "memoryAdapter.h"
#include <pthread.h>
#include "rawStreamParser.h"
#include "buffer_pool.h"
#include "preview_display.h"
/*config the rv2ve msg*/
#include "rpmsg_ve_config.h"
#include <hal_msgbox.h>
#include <openamp/sunxi_helper/openamp.h>

//#define VIDEO_FORMAT_H264 0x1
//#define VIDEO_FORMAT_H265 0x2
//#define VIDEO_FORMAT_AV1  0x3
#define RV_DEC_DEFAULT_PRIORITY ((configMAX_PRIORITIES) >> 3)
//#define RV_DEC_DEFAULT_PRIORITY ((configMAX_PRIORITIES >> 1))
#define MAX_PIC_NUM 4


/*define log func*/
//#define declog(fmt, args...) printf("[%s %d] " fmt"\r\n", __func__, __LINE__, ##args)
#define declog(fmt, args...)
#define loge(fmt, args...) printf("[%s %d][error] " fmt"\r\n", __func__, __LINE__, ##args)
#define logw(fmt, args...) printf("[%s %d][warning] " fmt"\r\n", __func__, __LINE__, ##args)
#define logi(fmt, args...) printf("[%s %d][info] " fmt"\r\n", __func__, __LINE__, ##args)

#define logd(fmt, args...)

#define logv(fmt, args...)

#define RUN_IN_RV 1

#if RUN_IN_RV
#define OUT_BUF_LEN 1920*1024
#define YUV_SIZE 1024*600
char *output = (char *)0x44020000;
#endif

int quit_flag = 0;
int disp_cnt = 0;
int dec_cnt = 0;
void *parser_thread, *dec_thread, *disp_thread, *monitor_thread;

typedef struct RvPacketS
{
    void *buf;
    void *ringBuf;
    int buflen;
    int ringBufLen;
//    int64_t pts;
//    int64_t length;
//    int64_t pcr;
}RvPacketT;

enum RV_DEC_STATUS
{
    RV_DEC_IDLE = 0,
    RV_DEC_RUN,
    RV_DEC_DONE,
    RV_DEC_QUIT,
};

typedef struct RvDecDemo
{
    /*codec config*/
    VideoDecoder *pVideoDec;
    int  nCodecFormat;
    struct ScMemOpsS* memops;
    int nVeFreq;
    int nOutputPixelFormat;
    /*parse config*/
    RawParserContext *pParser;
    pthread_mutex_t RVMutex;
    int QuitFlag;
    /*param for parallel*/
    int dec_cnt;
    int disp_cnt;
    enum RV_DEC_STATUS  parser_status;
    enum RV_DEC_STATUS dec_status;
    enum RV_DEC_STATUS  disp_status;
    /*pic param*/
    int first_frame_status;
    int nWidth;
    int nHeight;
}RvDecDemo;

RvDecDemo *RvDec;

typedef struct rpmsg_rvdec_priv
{
    int rpmsg_id;
    struct rpmsg_endpoint *ept;
    int is_unbound;
    int is_ready;
    int is_destroyed;
    int is_quit;
}rpmsg_rvdec;

rpmsg_rvdec *dec_msg;

extern int preview_output_exit(void);
void cmd_destroy_decoder(void);

/*rpmsg callback*/
static int rv2ve_ept_callback(struct rpmsg_endpoint *ept, void *data,
        size_t len, uint32_t src, void *priv)
{
    ve_packet *d = (ve_packet *)data;
    int ret = 0;

    logi("Recv data from ve, len:%zu, magic:%x, type:%d", len, d->magic, d->type);
    if (d->type == VE_RV_STOP_ACK) {
        if (dec_msg)
            dec_msg->is_quit = 1;
        else {
            loge("dec_msg context doesn't exist");
            ret = -1;
        }
    }
#if 0
    if (!RvDec || !RvDec->pVideoDec) {
        pkt.type = RV_DEC_IDLE;
    }
    ret = openamp_rpmsg_send(dec_msg->ept, (void *)&pkt, sizeof(pkt));
    if (ret < 0)
        loge("rpmsg send failed\n");

#endif
    return ret;
}

static void rv2ve_unbind_callback(struct rpmsg_endpoint *ept)
{
    logi("Remote endpoint is destroyed");
    dec_msg->is_unbound = 1;
}

static inline unsigned long read_timer(void)
{
	unsigned long  cnt = 0;
	asm volatile("rdtime %0\n"
			: "=r" (cnt)
			:
			: "memory");
	return cnt;
}
#if 1
static int initDecoder(RvDecDemo *Decoder)
{
    VConfig VideoConf;
    VideoStreamInfo VideoInfo;
    memset(&VideoConf, 0, sizeof(VConfig));
    memset(&VideoInfo, 0, sizeof(VideoStreamInfo));
    Decoder->memops = MemAdapterGetOpsS();
    if(Decoder->memops == NULL)
    {
        loge("memops is NULL");
        return -1;
    }
    CdcMemOpen(Decoder->memops);
    pthread_mutex_init(&Decoder->RVMutex, NULL);
    logd(" before CreateVideoDecoder() ");
    Decoder->pVideoDec = CreateVideoDecoder();
    if(Decoder->pVideoDec == NULL)
    {
        loge(" decoder demom CreateVideoDecoder() error ");
        return -1;
    }
    logd(" before InitializeVideoDecoder() ");

    /*set param defaulty (cannot be  custom)*/
    Decoder->nCodecFormat = VIDEO_FORMAT_H264; //* set directly
    Decoder->nOutputPixelFormat= PIXEL_FORMAT_YUV_PLANER_420; //* set directly
    Decoder->nVeFreq = 0;

    /*fill VideoInfo*/
    int nCodecFormat =  Decoder->nCodecFormat;
    if(nCodecFormat == VIDEO_FORMAT_H264)
    {
        VideoInfo.eCodecFormat = VIDEO_CODEC_FORMAT_H264;
        //Decoder->pParser->streamType = VIDEO_FORMAT_H264;
    }
    else if(nCodecFormat == VIDEO_FORMAT_H265)
    {
        VideoInfo.eCodecFormat = VIDEO_CODEC_FORMAT_H265;
      //  Decoder->pParser->streamType = VIDEO_FORMAT_H265;
    }
    else if(nCodecFormat == VIDEO_FORMAT_AV1)
    {
        VideoInfo.eCodecFormat = VIDEO_CODEC_FORMAT_H264;
        //Decoder->pParser->streamType = VIDEO_FORMAT_AV1;
    }

    logd("nCodecFormat = %d, 0x%x",nCodecFormat, VideoInfo.eCodecFormat);
    VideoConf.eOutputPixelFormat  = Decoder->nOutputPixelFormat;
    logd("eOutputPixelFormat = %d", VideoConf.eOutputPixelFormat);
#if 1
    VideoConf.nDeInterlaceHoldingFrameBufferNum = 2;
    VideoConf.nDisplayHoldingFrameBufferNum = 2;
    VideoConf.nRotateHoldingFrameBufferNum = 0;
    VideoConf.nDecodeSmoothFrameBufferNum = 2;
#else
    VideoConf.nDeInterlaceHoldingFrameBufferNum = 2;
    VideoConf.nDisplayHoldingFrameBufferNum = 2;
    VideoConf.nRotateHoldingFrameBufferNum = 0;
    VideoConf.nDecodeSmoothFrameBufferNum = 2;

#endif
    VideoConf.memops = Decoder->memops;
    VideoConf.nVeFreq = Decoder->nVeFreq;

    logd("**** set the ve freq = %d",VideoConf.nVeFreq);
    int nRet = InitializeVideoDecoder(Decoder->pVideoDec, &VideoInfo, &VideoConf);
//    int nRet = -1;
    logd(" after InitializeVideoDecoder() ");
    if(nRet != 0)
    {
        loge("decoder demom initialize video decoder fail.");
        DestroyVideoDecoder(Decoder->pVideoDec);
        Decoder->pVideoDec = NULL;
    }
    logd("InitializeVideoDecoder Ok!");
    return nRet;
}
#endif
int rv_dec_deinit()
{

    pthread_mutex_destroy(&RvDec->RVMutex);
#if 0
    if (parser_thread != NULL) {
        hal_thread_stop(parser_thread);
        parser_thread = NULL;
    }

    if (dec_thread != NULL) {
        hal_thread_stop(dec_thread);
        dec_thread = NULL;
    }

    if (disp_thread != NULL) {
        hal_thread_stop(disp_thread);
        disp_thread = NULL;
    }
#endif

    if (RvDec->pVideoDec) {
        DestroyVideoDecoder(RvDec->pVideoDec);
        RvDec->pVideoDec = NULL;
    }

    if (RvDec) {
        free(RvDec);
        RvDec = NULL;
    }

    return 0;
}

void ParserThread(void *param)
{
    VideoDecoder *pVideoDec;
    VideoStreamDataInfo  dataInfo;
    DataPacketT packet;
    memset(&packet, 0, sizeof(packet));
    int nRequestDataSize = 0;
    int nValidSize = 0;

    int packet_cnt = 0;

    pVideoDec = RvDec->pVideoDec;
    logd("start parser loop");

    RvDec->pParser = RawParserOpen(NULL);
    int nRet = RawParserProbe(RvDec->pParser);
    logd("ret of Probe:%d\n", nRet);
    /* start parser loop*/
    while (RvDec->parser_status != RV_DEC_QUIT) {
        if (RvDec->parser_status != RV_DEC_RUN) {
            usleep(20*1000);
            continue;
        }
        if (RawParserPrefetch(RvDec->pParser, &packet) != 0) {
            RvDec->parser_status = RV_DEC_QUIT;
            logi("eos...");
            break;
        }
        nRequestDataSize = packet.length;
        nValidSize = VideoStreamBufferSize(pVideoDec, 0) - VideoStreamDataSize(pVideoDec, 0);
        logd("nValidSize:%d, req buf size:%d", nValidSize, nRequestDataSize);
        if (nRequestDataSize > nValidSize) {
            usleep(20*1000);
            logw("ValidSize of buffer not enough,valid:%d, req_size:%d", nValidSize, nRequestDataSize);
            continue;
        }
        /* request buffer from video dec comp*/
        nRet = RequestVideoStreamBuffer(pVideoDec, nRequestDataSize, (char**)&packet.buf, &packet.buflen, (char**)&packet.ringBuf, &packet.ringBufLen, 0);
        logd("RequestVideoStreamBuffer:%d", nRet);

        if (nRet != 0) {
            usleep(50*1000);
            logw("ValidSize of buffer not enough,valid:%d, req_size:%d", nValidSize, nRequestDataSize);
            continue;
        }

        if (packet.buflen  < nRequestDataSize) {
            loge("Request Stream Buffer failed, ValidSize too small:%d",  packet.buflen);
            goto ParserThread_out;
        }

        /*start read the data and submit to decoder buf*/
        nRet = RawParserRead(RvDec->pParser, &packet);
        packet_cnt++;
        memset(&dataInfo, 0, sizeof(VideoStreamDataInfo));
        dataInfo.pData          = packet.buf;
        dataInfo.nLength      = packet.buflen;
        dataInfo.nPts          = packet.pts;
        dataInfo.nPcr          = packet.pcr;
        dataInfo.bIsFirstPart = 1;
        dataInfo.bIsLastPart = 1;
        dataInfo.bValid = 1;
        nRet = SubmitVideoStreamData(pVideoDec , &dataInfo, 0);
        if (nRet != 0) {
            loge("SubmitVideoStreamData failed:%d", nRet);
            goto ParserThread_out;
        }

    }//end of parser loop

ParserThread_out:
//    sleep(5);
    printf("exiting ParserThread...valid packet cnt:%d\n", packet_cnt);
#if 0
    while(1) {
        hal_msleep(10);
    }
#else
    hal_thread_stop(NULL);
#endif

}

void DecThread(void *param)
{
    int nRet = 0;
    int end_of_stream = 0;
    VideoDecoder *pVideoDec;
    VideoPicture* pPicture;
    int dec_index = 0;
    pVideoDec = RvDec->pVideoDec;

    /*loop of decode*/
    while (RvDec->dec_status != RV_DEC_QUIT) {
        if (RvDec->dec_status != RV_DEC_RUN) {
            usleep(10*1000);
            continue;
        }
        if ((RvDec->dec_cnt - RvDec->disp_cnt) > MAX_PIC_NUM) {
            logd("no space for decode frame");
            usleep(10*1000);
            continue;
        }
        if (RvDec->parser_status == RV_DEC_QUIT)
            end_of_stream = 1;
        /* check the buf len */
        nRet = DecodeVideoStream(pVideoDec, end_of_stream, 0, 0, 0);
        logd("DeocodeVideoStream ---%d \n", nRet);
        if (nRet == 5 && RvDec->first_frame_status == 0) {
	        logd("dec-->nRet = 5 -- a %ld ms \n", (read_timer()/24000));
        }
        if (end_of_stream == 1 && nRet == 5) {
            logi("decoder thread finish decoding");
            RvDec->dec_status = RV_DEC_QUIT;
        }

        /* request pic and display */
        pPicture = RequestPicture(pVideoDec, 0/*the major stream*/);
        if (pPicture == NULL) {
            logd("no pic\n");
        //    usleep(10*1000);
        }
        else {
            if (RvDec->first_frame_status == 0) {
                RvDec->nWidth = pPicture->nWidth;
                RvDec->nHeight = pPicture->nHeight;
                RvDec->first_frame_status = 1;//1 means dec init
	            logd("first pic after dec -- a %ld ms \n", (read_timer()/24000));

            }
            dec_index = (RvDec->dec_cnt%3)*pPicture->nHeight*pPicture->nWidth*3/2;
//            logd("output:from %p-----to %p------\n", output, output+pPicture->nWidth*pPicture->nHeight*3/2);
//            logd("nWidth:%d ... nHeight:%d\n", pPicture->nWidth, pPicture->nHeight);
            /*loop-save-pic tmp*/

            /* save the yuv420 data to phy-addr */
            memcpy(output+dec_index, pPicture->pData0, pPicture->nWidth*pPicture->nHeight);
            memcpy(output+dec_index+pPicture->nWidth*pPicture->nHeight, pPicture->pData1, pPicture->nWidth*pPicture->nHeight/2);
            hal_dcache_clean((unsigned long)output+dec_index, pPicture->nWidth*pPicture->nHeight*3/2);
            nRet = ReturnPicture(pVideoDec, pPicture); //tmp,seems like release pic
            if (nRet != 0)
                logw("return pic failed!");
            /*loop-save-pic tmp*/
            RvDec->dec_cnt++;
        }



        /*TMP: decode once and then exit*/
//        pthread_mutex_unlock(&RvDec->RVMutex);
        usleep(5*1000);
    }

//DecThread_out:
//    sleep(5);
    printf("exiting DecThread...valid frame cnt:%d\n", RvDec->dec_cnt);
#if 0
    while(1) {
        hal_msleep(10);
    }
#else
    hal_thread_stop(NULL);
#endif
}


void DispThread(void *param)
{
//    int nRet = 0;

    /*pic init*/
    struct buffer_node *frame = malloc(sizeof(struct buffer_node));
    memset(frame, 0 , sizeof(struct buffer_node));
    frame->phy_address = (void *)output;
    int pic_index = 0;
    int pic_size = 0;
	static int disp_fisrt = 0;
    /*wait for dec thread*/

    usleep(30*1000);
    while (1) {
        if (RvDec->disp_status == RV_DEC_RUN)
            break;

        if (RvDec->disp_status == RV_DEC_QUIT)
            goto DispThread_out;
        usleep(1*1000);
        logv("RvDec->disp_status:%d\n", RvDec->disp_status);
    }

	logd("six -- a %ld ms \n", (read_timer()/24000));
    while (RvDec->disp_status != RV_DEC_QUIT) {
        if (RvDec->disp_cnt >= RvDec->dec_cnt) {
            if (RvDec->dec_status == RV_DEC_QUIT)
                break;

            logd("no enough pic for disp, disp cnt that going to show:%d, dec_cnt:%d", RvDec->disp_cnt, RvDec->dec_cnt);
            usleep(10*1000);
            continue;
        }
        if (RvDec->first_frame_status == 1) {
            pic_size = RvDec->nWidth*RvDec->nHeight*3/2;
            RvDec->first_frame_status = 2;
			logd("first frame status has changed  %ld ms \n", (read_timer()/24000));
        }
        pic_index = (RvDec->disp_cnt%3)*pic_size;

        frame->phy_address = (void *)output+pic_index;
        preview_layer_config_update(frame);
        preview_display();
		if (disp_fisrt == 0) {
			disp_fisrt = 1;
			printf("first disp %ld ms \n", (read_timer()/24000));
		}
        RvDec->disp_cnt++;
        logv("display cnt:%d",RvDec->disp_cnt);
        usleep(1*1000);

    }
DispThread_out:

    free(frame);
    printf("exiting disp thread, diplay_cnt:%d\n", RvDec->disp_cnt);
	preview_output_exit();
#if 0
    while(1) {
        hal_msleep(10);
    }
#else
	cmd_destroy_decoder();
    hal_thread_stop(NULL);
#endif
}

void RpMsgThread(void *param)
{

    dec_msg = (rpmsg_rvdec*)malloc(sizeof(rpmsg_rvdec));
    if (!dec_msg) {
        loge("rpmsg context malloc failed!");
        goto out;
    }
    memset(dec_msg, 0 , sizeof(rpmsg_rvdec));

    logi("start openamp_init");
    if (openamp_init() != 0) {
        loge("Failed to init openamp framework");
        goto out;
    }
    logi("creating end point");

    /*TODO: set the name/demo/rpmsg_id/src_addr/dst_addr in a .h file*/
    dec_msg->rpmsg_id = 0;
    dec_msg->is_unbound = 0;
    dec_msg->ept = openamp_ept_open(RPMSG_VE_NAME, dec_msg->rpmsg_id, RPMSG_VE_SRC_ADDR, RPMSG_VE_DST_ADDR,
                    dec_msg, rv2ve_ept_callback, rv2ve_unbind_callback);
    //dec_msg->ept = openamp_ept_open(name, dec_msg->rpmsg_id, src_addr, dst_addr,
    //                dec_msg, rv2ve_ept_callback, rv2ve_unbind_callback);
    if(dec_msg->ept == NULL) {
        loge("Failed to Create Endpoint");
        goto out;
    }

    logi("success to create end point");

    while(1) {
        if (dec_msg->is_quit)
            goto destroy_ept;
        
        cmd_dec_report();
        usleep(200*1000);
    }
destroy_ept:
    cmd_destroy_rpmsg_ve();
out:
    hal_thread_stop(NULL);
}

extern void *ve_buffer_pool_mem_alloc_align(size_t size, size_t align);
extern void ve_buffer_pool_mem_free_align(void *ptr);
//extern int ion_alloc_init(void (*palloc)(size_t size, size_t align), 
//					void (*pfree)(void *ptr));
//int ion_alloc_init(void (*pfree)(void *ptr));
int ion_alloc_init(void* (*palloc)(size_t size, size_t align) , void (*pfree)(void *ptr));
int rv_dec_init()
{
    int nRet = 0;
	
	//ion_alloc_init(ve_buffer_pool_mem_free_align);
	ion_alloc_init(ve_buffer_pool_mem_alloc_align, ve_buffer_pool_mem_free_align);
	
    RvDec = (RvDecDemo *)malloc(sizeof(RvDecDemo));
    if (!RvDec) {
        loge("RvDec malloc failed!");
        return -1;
    }
    memset(RvDec, 0, sizeof(RvDecDemo));

    nRet = initDecoder(RvDec);
    logd("initDecoder:%d", nRet);
    if (nRet != 0) {
        loge("initDecoder failed");
        goto dec_init_failed;
    }
#if 0
    /*pthread dec para declare*/
    //int dec_thrd_ret = 0;
    pthread_t thrd_dec;
    pthread_attr_t attr_dec;
    pthread_attr_init(&attr_dec);
    struct sched_param sched_dec;
    sched_dec.sched_priority = 29;
    pthread_attr_setschedparam(&attr_dec, &sched_dec);
    pthread_attr_setstacksize(&attr_dec, 32768);
    pthread_attr_setdetachstate(&attr_dec, PTHREAD_CREATE_DETACHED);
    pthread_create(&thrd_dec, NULL, DecThread, NULL);
    pthread_setname_np(thrd_dec, "dec_thread");
    /*pthread disp para declare*/
    //int disp_thrd_ret = 0;
    pthread_t thrd_disp;
    pthread_attr_t attr_disp;
    pthread_attr_init(&attr_disp);
    struct sched_param sched_disp;
    sched_disp.sched_priority = 29;
    pthread_attr_setschedparam(&attr_disp, &sched_disp);
    pthread_attr_setstacksize(&attr_disp, 32768);
    pthread_attr_setdetachstate(&attr_disp, PTHREAD_CREATE_DETACHED);

    pthread_create(&thrd_disp, NULL, DispThread, NULL);
    pthread_setname_np(thrd_disp, "disp_thread");
#endif
    parser_thread = hal_thread_create(ParserThread, NULL, "parser_thread_hal", 1024*4, 29);
    if (parser_thread != NULL)
        hal_thread_start(parser_thread);
    dec_thread = hal_thread_create(DecThread, NULL, "dec_thread_hal", 1024*4, 29);
    if (dec_thread != NULL)
        hal_thread_start(dec_thread);
    disp_thread = hal_thread_create(DispThread, NULL, "disp_thread_hal", 1024*4, 29);
    if (disp_thread != NULL)
        hal_thread_start(disp_thread);

 //   pthread_join(thrd_dec, (void **)&dec_thrd_ret);
 //   pthread_join(thrd_disp, (void **)&disp_thrd_ret);



    return nRet;

dec_init_failed:
    if (RvDec->pVideoDec) {
        DestroyVideoDecoder(RvDec->pVideoDec);
        RvDec->pVideoDec = NULL;
    }

    if (RvDec) {
        free(RvDec);
        RvDec = NULL;
    }
    return -1;

}

void start_decode()
{
    if (RvDec == NULL) {
        loge("RvDec not exist!");
        return;
    }
    pthread_mutex_lock(&RvDec->RVMutex);
    RvDec->dec_status = RV_DEC_RUN;
    RvDec->parser_status = RV_DEC_RUN;
    pthread_mutex_unlock(&RvDec->RVMutex);
    logd("start decode\n");
    return;
}

void start_display()
{
    if (RvDec == NULL) {
        loge("RvDec not exist!");
        return;
    }
    pthread_mutex_lock(&RvDec->RVMutex);
    RvDec->disp_status = RV_DEC_RUN;
    pthread_mutex_unlock(&RvDec->RVMutex);
    logd("run display thread");

}

int cmd_rv_decode(int argc, char *argv[])
{
    int nRet = 0;
    /* dec init*/
    /*create dec_thread*/
    nRet = rv_dec_init();
    /* start decode */
    start_decode();
    start_display();
    logd("DecThread Done");
    return nRet;
}
FINSH_FUNCTION_EXPORT_CMD(cmd_rv_decode, rv_decode, rv decode test demo);
void cmd_destroy_decoder()
{
    pthread_mutex_lock(&RvDec->RVMutex);
    RvDec->dec_status = RV_DEC_QUIT;
    RvDec->disp_status = RV_DEC_QUIT;
    pthread_mutex_unlock(&RvDec->RVMutex);
    rv_dec_deinit();

    /*try to connect linux-ve by rpmsg*/
    cmd_dec2ve();

}
FINSH_FUNCTION_EXPORT_CMD(cmd_destroy_decoder, destroy_decoder, rv decode destroy);

void cmd_dec2ve()
{
    if (monitor_thread == NULL) {
        monitor_thread = hal_thread_create(RpMsgThread, NULL, "rv_decode_rpmsg_init", 1024*4, 29);
        if (monitor_thread != NULL)
            hal_thread_start(monitor_thread);
    } else
        loge("monitor_thread already exist");

}
FINSH_FUNCTION_EXPORT_CMD(cmd_dec2ve, dec2ve, start rpmsg for ve);

int cmd_dec_report()
{
    if (!dec_msg) {
        loge("rpmsg service for rv-dec doesn't exist");
        return -1;
    }
    ve_packet pkt;
    pkt.magic = VE_PACKET_MAGIC;
    pkt.type = VE_RV_START;
    int ret = 0;

    if (!RvDec || !RvDec->pVideoDec) {
        pkt.type = VE_RV_STOP;
    }
    logi("prepare to send msg, pkt.magic:%x, pkt.type:%d", pkt.magic, pkt.type);
    ret = openamp_rpmsg_send(dec_msg->ept, (void *)&pkt, sizeof(pkt));
    if (ret < 0) {
        loge("rpmsg send failed\n");
        return ret;
    }
    return 0;
}

void cmd_destroy_rpmsg_ve()
{
    if (dec_msg != NULL) {
        openamp_ept_close(dec_msg->ept);
        free(dec_msg);
        dec_msg = NULL;
        printf("rv2ve_msg destroy done\n");
    } else
        loge("rv2ve_msg is not exist!");
}
FINSH_FUNCTION_EXPORT_CMD(cmd_destroy_rpmsg_ve, destroy_rpmsg_ve, rp msg for ve destroy);
