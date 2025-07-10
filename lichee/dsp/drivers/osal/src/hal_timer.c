#include <string.h>

#include <hal_osal.h>
#include <FreeRTOS.h>
#include <timers.h>

typedef struct _os_timer_data_t
{
    timeout_func callback;
    void *arg;
} os_timer_data_t;

static void os_timer_common_callback(TimerHandle_t xTimer)
{
    os_timer_data_t *priv;

    priv = pvTimerGetTimerID(xTimer);
    if (priv && priv->callback)
    {
        priv->callback(priv->arg);
    }
    else
    {
        hal_log_warn("Invalid timer callback\n");
    }
}

osal_timer_t osal_timer_create(const char *name,
                               timeout_func timeout,
                               void *parameter,
                               unsigned int tick,
                               unsigned char flag)
{
    os_timer_data_t *priv;
    priv = hal_malloc(sizeof(os_timer_data_t));
    if (priv == NULL)
    {
        return NULL;
    }
    memset(priv, 0, sizeof(os_timer_data_t));
    priv->callback = timeout;
    priv->arg = parameter;
    return xTimerCreate(name, tick, flag & OSAL_TIMER_FLAG_PERIODIC ? pdTRUE : pdFALSE, priv, os_timer_common_callback);
}

hal_status_t osal_timer_delete(osal_timer_t timer)
{
    BaseType_t ret;
    os_timer_data_t *priv;

    priv = pvTimerGetTimerID((TimerHandle_t)timer);

    ret = xTimerDelete(timer, portMAX_DELAY);
    if (ret != pdPASS)
    {
        hal_log_err("err %d\n", ret);
        return HAL_ERROR;
    }

    hal_free(priv);
    return HAL_OK;
}

hal_status_t osal_timer_start(osal_timer_t timer)
{
    BaseType_t ret;
    BaseType_t taskWoken;

    if (hal_interrupt_get_nest())
    {
        taskWoken = pdFALSE;
        ret = xTimerStartFromISR(timer, &taskWoken);
        if (ret != pdPASS)
        {
            hal_log_err("err %d\n", ret);
            return HAL_ERROR;
        }
        portYIELD_FROM_ISR(taskWoken);
    }
    else
    {
        ret = xTimerStart(timer, 0);
        if (ret != pdPASS)
        {
            hal_log_err("err %d\n", ret);
            return HAL_ERROR;
        }
    }

    return HAL_OK;
}

hal_status_t osal_timer_stop(osal_timer_t timer)
{
    BaseType_t ret;
    BaseType_t taskWoken;

    if (hal_interrupt_get_nest())
    {
        taskWoken = pdFALSE;
        ret = xTimerStopFromISR(timer, &taskWoken);
        if (ret != pdPASS)
        {
            hal_log_err("err %d\n", ret);
            return HAL_ERROR;
        }
        portYIELD_FROM_ISR(taskWoken);
    }
    else
    {
        ret = xTimerStop(timer, portMAX_DELAY);
        if (ret != pdPASS)
        {
            hal_log_err("err %d\n", ret);
            return HAL_ERROR;
        }
    }

    return HAL_OK;
}

hal_status_t osal_timer_control(osal_timer_t timer, int cmd, void *arg)
{
    BaseType_t ret = pdPASS;
    BaseType_t taskWoken;
    BaseType_t periodTick;

    if (hal_interrupt_get_nest())
    {
        taskWoken = pdFALSE;
        switch (cmd)
        {
            case OSAL_TIMER_CTRL_SET_TIME:
                periodTick = *(BaseType_t *)(arg);
                ret = xTimerChangePeriodFromISR(timer, periodTick, &taskWoken);
                break;
            default:
                break;
        }
        if (ret != pdPASS)
        {
            hal_log_err("err %d\n", ret);
            return HAL_ERROR;
        }
        portYIELD_FROM_ISR(taskWoken);
    }
    else
    {
        switch (cmd)
        {
            case OSAL_TIMER_CTRL_SET_TIME:
                periodTick = *(BaseType_t *)(arg);
                ret = xTimerChangePeriod(timer, periodTick, 0);
                break;
            default:
                break;
        }
        if (ret != pdPASS)
        {
            hal_log_err("err %d\n", ret);
            return HAL_ERROR;
        }
    }

    return HAL_OK;
}
