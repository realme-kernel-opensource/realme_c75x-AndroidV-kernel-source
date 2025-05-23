/*
 *-----------------------------------------------------------------------------
 * The confidential and proprietary information contained in this file may
 * only be used by a person authorised under and to the extent permitted
 * by a subsisting licensing agreement from  CHIPSEA.
 *
 *              (C) COPYRIGHT 2020 SHENZHEN CHIPSEA TECHNOLOG_ERRIES CO.,LTD.
 *                  ALL RIGHTS RESERVED
 *
 * This entire notice must be reproduced on all copies of this file
 * and copies of this file may only be made by a person if such person is
 * permitted to do so under the terms of a subsisting license agreement
 * from CHIPSEA.
 *
 *        Release Information : CSA37F71 chip forcetouch fw linux driver source file
 *        version : v1.x
 *-----------------------------------------------------------------------------
 */

#include "cs_press_f71.h"

//#define ALIENTEK
#ifndef ALIENTEK
//#include <soc/oplus/system/boot_mode.h>
#include <linux/thermal.h>
#endif
const char *cs_driver_ver = "1.13";

#define PROC_FOPS_NUM  28
#define PROC_NAME_LEN  32

#ifndef ALIENTEK
#define FOPS_ARRAY(_open, _write) \
{\
    .proc_open = _open,\
    .proc_read = seq_read,\
    .proc_release = single_release,\
    .proc_write = _write,\
    .proc_lseek = seq_lseek,\
}
#else
#define FOPS_ARRAY(_open, _write) \
{\
    .owner = THIS_MODULE,\
    .open = _open,\
    .read = seq_read,\
    .llseek = seq_lseek,\
    .release = single_release,\
    .write = _write,\
}
#endif
LIST_HEAD(gScenes);
LIST_HEAD(gSceneClients);

static struct mutex    i2c_rw_lock;
static DEFINE_MUTEX(i2c_rw_lock);

static struct mutex    press_lock;
static DEFINE_MUTEX(press_lock);

static struct cs_press_t g_cs_press;

#define NORMAL_GEAT_2LEVLE_EN 1
#define FW_ACTION_HOTPLUG 1 

static int game_geat[PRESS_NUM] = {50, 80, 120};
#if NORMAL_GEAT_2LEVLE_EN
#if 0
static int normal_left_geat[PRESS_NUM] = {50, 80};
static int normal_right_geat[PRESS_NUM] = {80, 110};
#endif
#else
static int normal_left_geat = 50;
static int normal_right_geat = 80;
#endif

#ifdef INT_SET_EN
static DECLARE_WAIT_QUEUE_HEAD(cs_press_waiter);
static int cs_press_int_flag;
int cs_press_irq_gpio;
int cs_press_irq_num;

/*1 enable,0 disable,  need to confirm after register eint*/
static int cs_irq_flag = 1;
struct input_dev *cs_input_dev;
#endif
/******* fuction definition start **********/
#ifdef INT_SET_EN
static void cs_irq_enable(void);
static void cs_irq_disable(void);
#endif
//static void ftm_boot_mode_check(void);
char cs_press_get_shell_temp(int *shell_temp);
void set_mode_from_client_list(struct work_struct *worker);
void scene_para_copy(struct scene_para_t*dst_scene, struct scene_para_t*src_scene);
static int cs_insert_scene_client(struct scene_client_t);
int parse_sence_str(char*,  struct scene_client_t *);
char cs_press_clear_reset_register(void);

void cs_press_struct_init(void)
{
    g_cs_press.update_type = HIGH_VER_FILE_UPDATE;/*1:force update, 0:to higher ver update*/
    g_cs_press.updating_flag = 0;
    g_cs_press.game_status = 0;                   /*bit0 game,bit1 setting,bit2 camera*/
    g_cs_press.normal_left_geat_val = 50;
    g_cs_press.normal_right_geat_val = 80;
    g_cs_press.cs_shell_themal_init_flag = 1;
    g_cs_press.cs_shell_themal_enable = 0;

    g_cs_press.charge_flag = 0;
}

/**@brief     read n reg datas from reg_addr.
 * @param[in]  dev:      csa37f71 device struct.
 * @param[in]  reg_addr: register addr
 * @param[in]  len:      read data lenth
 * @param[out] buf:     read data buffer addr.
 * @return     i2c_transfer status
 *             OK:      return num of send data
 *             err:     return err code
 */
static int cs_i2c_read_bytes(struct i2c_client *client, unsigned char reg_addr, void *buf, unsigned int len)
{
    int ret = 0;
    struct i2c_msg msg[2];
    struct i2c_adapter *adapter = client->adapter;
    int i = 5;

    if(buf == NULL){
        LOG_ERR("input buf is NULL\n");
        return -1;
    }
    if(len == 0){
        LOG_ERR("input len is invalid,len = 0\n");
        return -1;
    }
    /* msg[0] send first addr for read */
    msg[0].addr = client->addr;            /* csa37f71 addr */
    msg[0].flags = 0;                      /* send flag */
    msg[0].buf = &reg_addr;                /* reg addr */
    msg[0].len = 1;                        /* reg lenth */

    /* msg[1]read data */
    msg[1].addr = client->addr;
    msg[1].flags = I2C_M_RD;               /* read flag */
    msg[1].buf = buf;
    msg[1].len = len;
    mutex_lock(&i2c_rw_lock);
    do{
        ret = i2c_transfer(adapter, msg, sizeof(msg) / sizeof(struct i2c_msg));
        if(ret <= 0){
            LOG_ERR("i2c read failed! err_code:%d\n",ret);
        }else{
            break;
        }
        i--;
    }while(i > 0);
    mutex_unlock(&i2c_rw_lock);
    return ret;
}

 /**@brief    send multi datas to reg_addr.
 * @param[in]  dev:      csa37f71 device struct.
 * @param[in]  reg_addr: register addr
 * @param[in]  len:      send data lenth
 * @param[out] buf:     send data buffer addr.
 * @return     i2c_transfer status
 *             OK:      return num of send data
 *             err:     return err code
 */
static int cs_i2c_write_bytes(struct i2c_client *client, unsigned char reg_addr, unsigned char *buf, unsigned char len)
{
    int ret = 0;
    unsigned char *t_buf = NULL;
    struct i2c_msg msg;
    struct i2c_adapter *adapter = client->adapter;
    int i= 5;

    if(buf == NULL){
        LOG_ERR("input buf is NULL\n");
        return -1;
    }
    if(len == 0){
        LOG_ERR("input len is invalid,len = 0\n");
        return -1;
    }
    t_buf = (unsigned char *)kmalloc(len + sizeof(reg_addr), GFP_KERNEL );
    if (!t_buf){
        LOG_ERR("kmalloc  failed\n");
        return -1;
    }
    t_buf[0] = reg_addr;            /* register first addr */
    memcpy(&t_buf[1],buf,len);      /* copy send data to b[256]*/

    msg.addr = client->addr;
    msg.flags = 0;                  /* write flag*/
    msg.buf = t_buf;
    msg.len = len + 1;
    mutex_lock(&i2c_rw_lock);
    do{
        ret = i2c_transfer(adapter, &msg, 1);
        if(ret <= 0){
            LOG_ERR("i2c write failed! err_code:%d\n", ret);
        }else if(ret > 0){
            break;
        }
        i--;
    }while(i > 0);
    mutex_unlock(&i2c_rw_lock);
    kfree(t_buf);
    return ret;
}

/**@brief      read n reg datas from reg_addr.
 * @param[in]  dev:         csa37f71 device struct.
 * @param[in]  reg_addr:    register addr,addr type is word type
 * @param[in]  len:         read data lenth
 * @param[out] buf:         read data buffer addr.
 * @return     i2c_transfer status
 *             OK:      return num of send data
 *             err:     return err code
 */
static int cs_i2c_read_bytes_by_u16_addr(struct i2c_client *client, unsigned short reg_addr, void *buf, int len)
{
    int ret = 0;
    unsigned char reg16[2];
    struct i2c_msg msg[2];
    struct i2c_adapter *adapter = client->adapter;

    if(buf == NULL){
        LOG_ERR("input buf is NULL\n");
        return -1;
    }
    if(len == 0){
        LOG_ERR("input len is invalid,len = 0\n");
        return -1;
    }

    reg16[0] = (reg_addr >> 8)&0xff;
    reg16[1] = reg_addr & 0xff;
    /* msg[0] send first addr for read */
    msg[0].addr = client->addr;            /* csa37f71 addr */
    msg[0].flags = 0;                      /* send flag */
    msg[0].buf = reg16;                    /* reg addr */
    msg[0].len = sizeof(reg16);            /* reg lenth:2*byte */

    /* msg[1]read data */
    msg[1].addr = client->addr;
    msg[1].flags = I2C_M_RD;               /* read flag*/
    msg[1].buf = buf;
    msg[1].len = len;

    ret = i2c_transfer(adapter, msg, 2);
    if(ret == 2) {
        ret = 0;
    } else {
        LOG_ERR("i2c read failed! err_code:%d\n",ret);
    }
    return ret;
}

 /**@brief    send multi datas to reg_addr.
 * @param[in] dev:      csa37f71 device struct.
 * @param[in] reg_addr: register addr
 * @param[in] len:      send data lenth
 * @param[out] buf:     send data buffer addr.
 * @return     i2c_transfer status
 *             OK:   return num of send data
 *             err:  return err code
 */
static int cs_i2c_write_bytes_by_u16_addr(struct i2c_client *client, unsigned short reg_addr, unsigned char *buf, unsigned char len)
{
    int ret = 0;
    unsigned char *t_buf = NULL;
    struct i2c_msg msg;
    struct i2c_adapter *adapter = client->adapter;

    if(buf == NULL){
        LOG_ERR("input buf is NULL\n");
        return -1;
    }
    if(len == 0){
        LOG_ERR("input len is invalid,len = 0\n");
        return -1;
    }
    t_buf = (unsigned char *)kmalloc(len + sizeof(reg_addr), GFP_KERNEL );
    if (!t_buf){
        LOG_ERR("kmalloc  failed\n");
        return -1;
    }

    t_buf[0] = (reg_addr >> 8)&0xff; /* register first addr */
    t_buf[1] = reg_addr & 0xff;
    memcpy(&t_buf[2],buf,len);       /* copy send data to b[256]*/

    msg.addr = client->addr;
    msg.flags = 0;                   /* write flag*/

    msg.buf = t_buf;
    msg.len = len + 2;
    ret = i2c_transfer(adapter, &msg, 1);
    if(ret < 0)
    {
        LOG_ERR("i2c write failed! err_code:%d\n",ret);
    }
    kfree(t_buf);
    return ret;
}

/**
  * @brief      iic write funciton
  * @param[in]  regAddress: reg data
  * @param[in]  dat:        point to data to write
  * @param[in]  length:     write data length
  * @retval     0:success, < 0: fail
  */
static int cs_press_iic_write(unsigned char regAddress, unsigned char *dat, unsigned int length)
{
    int ret = 0;
    int i = 0;
#if defined(CONFIG_OPLUS_FEATURE_FEEDBACK) || defined(CONFIG_OPLUS_FEATURE_FEEDBACK_MODULE)
    char payload[1024] = {0x00};
#endif

    if(dat == NULL){
        LOG_ERR("input buf is NULL\n");
        return -1;
    }
    if(length == 0){
        LOG_ERR("input len is invalid,len = 0\n");
        return -1;
    }
    ret = cs_i2c_write_bytes(g_cs_press.client, regAddress, dat, length);
    if(ret < 0){
#if defined(CONFIG_OPLUS_FEATURE_FEEDBACK) || defined(CONFIG_OPLUS_FEATURE_FEEDBACK_MODULE)
        scnprintf(payload, sizeof(payload),
                   "NULL$$EventField@@I2cWrite$$FieldData@@Err%d$$detailData@@%u[%*ph]%d",
                   ret, regAddress, length, dat, length);
        oplus_kevent_fb(PSW_BSP_KEYPAD, CS_PRESS_FB_BUS_TRANS_TYPE, payload);
#endif
        i = g_cs_press.bus_error_cnt % BUS_ERROR_MSG_CNT;
        memset(g_cs_press.bus_error_msg[i], 0, BUS_ERROR_MSG_SIZE);
        scnprintf(g_cs_press.bus_error_msg[i], sizeof(g_cs_press.bus_error_msg[i]) - 1,
                   "%d WrErr%d %u[%*ph]%u", g_cs_press.bus_error_cnt, ret, regAddress, length, dat, length);
        g_cs_press.bus_error_cnt++;
        return -1;
    }
    return 0;
}

/**
  * @brief       iic read funciton
  * @param[in]   regAddress: reg data,
  * @param[out]  dat:        read data buffer,
  * @param[in]   length:     read data length
  * @retval      0:success, -1: fail
  */
static int cs_press_iic_read(unsigned char regAddress, unsigned char *dat, unsigned int length)
{
    int ret = 0;
    int i = 0;
#if defined(CONFIG_OPLUS_FEATURE_FEEDBACK) || defined(CONFIG_OPLUS_FEATURE_FEEDBACK_MODULE)
    char payload[1024] = {0x00};
#endif

    if(dat == NULL){
        LOG_ERR("input buf is NULL\n");
        return -1;
    }
    if(length == 0){
        LOG_ERR("input len is invalid,len = 0\n");
        return -1;
    }
    /* user program */
    ret = cs_i2c_read_bytes(g_cs_press.client, regAddress, dat, length);
    if(ret < 0){
        LOG_ERR("cs_i2c_read_bytes err\n");
#if defined(CONFIG_OPLUS_FEATURE_FEEDBACK) || defined(CONFIG_OPLUS_FEATURE_FEEDBACK_MODULE)
        scnprintf(payload, sizeof(payload),
                   "NULL$$EventField@@I2cRead$$FieldData@@Err%d$$detailData@@%u[%*ph]%d",
                   ret, regAddress, length, dat, length);
        oplus_kevent_fb(PSW_BSP_KEYPAD, CS_PRESS_FB_BUS_TRANS_TYPE, payload);
#endif
        i = g_cs_press.bus_error_cnt % BUS_ERROR_MSG_CNT;
        memset(g_cs_press.bus_error_msg[i], 0, BUS_ERROR_MSG_SIZE);
        scnprintf(g_cs_press.bus_error_msg[i], sizeof(g_cs_press.bus_error_msg[i]) - 1,
                   "%d RdErr%d %u[%*ph]%u", g_cs_press.bus_error_cnt, ret, regAddress, length, dat, length);
        g_cs_press.bus_error_cnt++;
        return -1;
    }
    return 0;
}

/**
  * @brief  iic write funciton
  * @param[in]  regAddress: reg data,
  * @param[in]  *dat:       point to data to write,
  * @param[in]  length:     write data length
  * @retval 0:success, -1: fail
  */
static int cs_press_iic_write_double_reg(unsigned short regAddress, unsigned char *dat, unsigned int length)
{
    int ret = 0;
    int i = 0;
#if defined(CONFIG_OPLUS_FEATURE_FEEDBACK) || defined(CONFIG_OPLUS_FEATURE_FEEDBACK_MODULE)
    char payload[1024] = {0x00};
#endif

    if(dat == NULL){
        LOG_ERR("input buf is NULL\n");
        return -1;
    }
    if(length == 0){
        LOG_ERR("input len is invalid,len = 0\n");
        return -1;
    }
    /* user program*/
    ret = cs_i2c_write_bytes_by_u16_addr(g_cs_press.client, regAddress, dat,length);
    if (ret < 0) {
        LOG_ERR("cs_i2c_read_bytes err\n");
#if defined(CONFIG_OPLUS_FEATURE_FEEDBACK) || defined(CONFIG_OPLUS_FEATURE_FEEDBACK_MODULE)
        scnprintf(payload, sizeof(payload),
                   "NULL$$EventField@@I2cWriteDouble$$FieldData@@Err%d$$detailData@@%u[%*ph]%d",
                   ret, regAddress, length, dat, length);
        oplus_kevent_fb(PSW_BSP_KEYPAD, CS_PRESS_FB_BUS_TRANS_TYPE, payload);
#endif
        i = g_cs_press.bus_error_cnt % BUS_ERROR_MSG_CNT;
        memset(g_cs_press.bus_error_msg[i], 0, BUS_ERROR_MSG_SIZE);
        scnprintf(g_cs_press.bus_error_msg[i], sizeof(g_cs_press.bus_error_msg[i]) - 1,
                   "%d WrDbErr%d %u[%*ph]%u", g_cs_press.bus_error_cnt, ret, regAddress, length, dat, length);
        g_cs_press.bus_error_cnt++;
    }
    return ret;
}

/**
  * @brief      iic read funciton
  * @param[in]  regAddress: reg data,
  * @param[out] *dat:       read data buffer,
  * @param[in]  length:     read data length
  * @retval 0:success, -1: fail
  */
static int cs_press_iic_read_double_reg(unsigned short regAddress, unsigned char *dat, unsigned int length)
{
    int ret = 0;
    int i = 0;
#if defined(CONFIG_OPLUS_FEATURE_FEEDBACK) || defined(CONFIG_OPLUS_FEATURE_FEEDBACK_MODULE)
    char payload[1024] = {0x00};
#endif

    if(dat == NULL){
        LOG_ERR("input buf is NULL\n");
        return -1;
    }
    if(length == 0){
        LOG_ERR("input len is invalid,len = 0\n");
        return -1;
    }
    /* user program*/
    ret = cs_i2c_read_bytes_by_u16_addr(g_cs_press.client, regAddress, dat, length);
    if (ret < 0) {
        LOG_ERR("cs_i2c_read_double_reg err\n");
#if defined(CONFIG_OPLUS_FEATURE_FEEDBACK) || defined(CONFIG_OPLUS_FEATURE_FEEDBACK_MODULE)
        scnprintf(payload, sizeof(payload),
                   "NULL$$EventField@@I2cReadDouble$$FieldData@@Err%d$$detailData@@%u[%*ph]%d",
                   ret, regAddress, length, dat, length);
        oplus_kevent_fb(PSW_BSP_KEYPAD, CS_PRESS_FB_BUS_TRANS_TYPE, payload);
#endif
        i = g_cs_press.bus_error_cnt % BUS_ERROR_MSG_CNT;
        memset(g_cs_press.bus_error_msg[i], 0, BUS_ERROR_MSG_SIZE);
        scnprintf(g_cs_press.bus_error_msg[i], sizeof(g_cs_press.bus_error_msg[i]) - 1,
                   "%d RdDbErr%d %u[%*ph]%u", g_cs_press.bus_error_cnt, ret, regAddress, length, dat, length);
        g_cs_press.bus_error_cnt++;
    }
    return ret;
}

/**
  * @brief      delay function
  * @param[in]  time_ms: delay time, unit:ms
  * @retval     None
  */
static void cs_press_delay_ms(unsigned int time_ms)
{
    msleep(time_ms);
}

#if !RSTPIN_RESET_ENABLE
/**
  * @brief  cs_press_power_up
  * @param  None
  * @retval None
  */
static void cs_press_power_up(void)
{
    // user program
    #if 0
    if(gpio_is_valid(g_cs_press.power_gpio)){
        gpio_set_value(g_cs_press.power_gpio, RST_GPIO_HIGH);
    }else{
        LOG_ERR("gpio rst is invalid\n");
    }
    #endif
}

/**
  * @brief  ic power down function
  * @param  None
  * @retval None
  */
static void cs_press_power_down(void)
{
    // user program
    #if 0
    if(gpio_is_valid(g_cs_press.power_gpio)){
        gpio_set_value(g_cs_press.power_gpio, RST_GPIO_LOW);
    }else{
        LOG_ERR("gpio rst is invalid\n");
    }
    #endif
}
#endif

#if RSTPIN_RESET_ENABLE
/**
  * @brief  ic rst pin set high
  * @param  None
  * @retval None
  */
static void cs_press_rstpin_high(void)
{
    /* user program*/
    #if 1
    if(gpio_is_valid(g_cs_press.rst_gpio)){
    LOG_ERR("gpio_set_value %d HIGH\n", g_cs_press.rst_gpio);
        gpio_set_value(g_cs_press.rst_gpio, RST_GPIO_HIGH);
    }else{
        LOG_ERR("gpio rst is invalid\n");
    }
    #endif
}

/**
  * @brief  ic rst pin set low
  * @param  None
  * @retval None
  */
static void cs_press_rstpin_low(void)
{
    /* user program*/
    #if 1
    if(gpio_is_valid(g_cs_press.rst_gpio)){
    LOG_ERR("gpio_set_value %d LOW\n", g_cs_press.rst_gpio);
        gpio_set_value(g_cs_press.rst_gpio, RST_GPIO_LOW);
    }else{
        LOG_ERR("gpio rst is invalid\n");
    }
    #endif
}
#endif
/**
  * @brief      iic function test
  * @param[in]  test_data: test data
  * @retval     0:success, -1: fail
  */
int cs_press_iic_rw_test(unsigned char test_data)
{
    int ret = 0;
    unsigned char retry = RETRY_NUM;
    unsigned char read_data = 0;
    unsigned char write_data = test_data;

    do
    {
        cs_press_iic_write(AP_RW_TEST_REG, &write_data, 1);
        cs_press_iic_read(AP_RW_TEST_REG, &read_data, 1);
        ret = 0;
        retry--;
        if(read_data != write_data)
        {
            ret = -1;
            LOG_ERR("iic test failed,w:%d,rd:%d  %d\n",write_data,read_data,ret);
            cs_press_delay_ms(1);
        }else{
            LOG_INFO("iic test ok,w:%d,rd:%d\n",write_data,read_data);
            retry = 0;
        }

    }while(retry > 0);
    return ret;
}

/**
  * @brief  wakeup iic
  * @param  None
  * @retval 0:success, -1: fail
  */
static char cs_press_wakeup_iic(void)
{
    int ret = 0;

    ret = cs_press_iic_rw_test(0x67);
    if (g_cs_press.suspend_lock) {
        __pm_wakeup_event(g_cs_press.suspend_lock, 2000);
    }
    return (char)ret;
}

int cs_press_set_trigger_strength(trigger_strength_config strength_cfg)
{
    int ret = 0;
    unsigned char cfg[12] = { 0 };
    unsigned char reg_active[2] = {AP_RW_SCANT_PERIOD_REG, 0xa2};
    int retry = 20;

    cfg[0] = strength_cfg.light_tap_down_strength;
    cfg[1] = strength_cfg.light_tap_up_strength;
    cfg[2] = strength_cfg.heavy_tap_down_strength;
    cfg[3] = strength_cfg.heavy_tap_up_strength;
    cfg[4] = strength_cfg.light_swipe_down_strength;
    cfg[5] = strength_cfg.light_swipe_up_strength;
    cfg[6] = strength_cfg.heavy_swipe_down_strength;
    cfg[7] = strength_cfg.heavy_swipe_up_strength;
    cfg[8] = strength_cfg.area_1;
    cfg[9] = strength_cfg.area_2;
    cfg[10] = strength_cfg.long_tap_judge_time;
    cfg[11] = strength_cfg.muti_tap_judge_time;

    LOG_INFO("%s: cfg = [%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u].\n", __func__,
            cfg[0], cfg[1], cfg[2], cfg[3], cfg[4], cfg[5],
            cfg[6], cfg[7], cfg[8], cfg[9], cfg[10], cfg[11]);

    ret = cs_press_iic_write(AP_RW_SCANT_PERIOD_REG, cfg, 12);
    if (ret < 0) {
        LOG_ERR("IIC write AP_RW_SCANT_PERIOD_REG failed!!\n");
        return ret;
    }

    ret = cs_press_iic_write(0x01, reg_active, 2);
    if (ret < 0) {
        LOG_ERR("IIC write 0x01 failed!!\n");
        return ret;
    }

    while ((reg_active[0] || reg_active[1]) && retry) {
        retry--;
        cs_press_delay_ms(2);
        ret = cs_press_iic_read(0x01, reg_active, 2);
        if (ret < 0) {
            LOG_ERR("%s: IIC read 0x01 failed!!\n", __func__);
            return ret;
        }
        LOG_INFO("read 0x01 [0x%02x 0x%02x]\n", reg_active[0], reg_active[1]);
    }
    if (!retry) {
        LOG_ERR("%s: IIC read 0x01 retry over times!!\n", __func__);
    }
    LOG_INFO("exit %s\n", __func__);
    return ret;
}

int cs_press_set_mode(int mode)
{
    int ret = 0;
#ifdef CAMERA_KEY
    unsigned char ic_mode = 0;
    unsigned char reg_active[2] = {AP_RD_APPLICATION_SATUS_REG, 0xab};
    int retry = 20;

    if (g_cs_press.quick_on_closed && (mode != CAMERA_KEY_CAMERA_MODE)) {
        ic_mode = CAMERA_KEY_IC_NONE_EVENT_MODE;
    } else {
        switch (mode) {
        case CAMERA_KEY_DEFAULT_MODE:
            ic_mode = CAMERA_KEY_IC_DEFAULT_EVENT_MODE;
            break;
        case CAMERA_KEY_CAMERA_MODE:
            ic_mode = CAMERA_KEY_IC_REALTIME_EVENT_MODE;
            break;
        case CAMERA_KEY_POPUP_MODE:
            ic_mode = CAMERA_KEY_IC_DELAY_EVENT_MODE;
            break;
        case CAMERA_KEY_SLEEP_MODE:
            ic_mode = CAMERA_KEY_IC_SLEEP_EVENT_MODE;
            break;
        default:
            break;
        }
    }
    LOG_INFO("%s: mode = %d (ic mode = 0x%02x%s)\n", __func__,
                mode, ic_mode, g_cs_press.quick_on_closed ? ", quick on closed..." : "");

    ret = cs_press_iic_write(AP_RD_APPLICATION_SATUS_REG, &ic_mode, 1);
    if (ret < 0) {
        LOG_ERR("IIC write AP_RD_APPLICATION_SATUS_REG 0x%02x failed!!\n", ic_mode);
        return ret;
    }

    ret = cs_press_iic_write(0x01, reg_active, 2);
    if (ret < 0) {
        LOG_ERR("IIC write 0x01 failed!!\n");
        return ret;
    }

    while ((reg_active[0] || reg_active[1]) && retry) {
        retry--;
        cs_press_delay_ms(2);
        ret = cs_press_iic_read(0x01, reg_active, 2);
        if (ret < 0) {
            LOG_ERR("%s: IIC read 0x01 failed!!\n", __func__);
            return ret;
        }
        LOG_INFO("read 0x01 [0x%02x 0x%02x]\n", reg_active[0], reg_active[1]);
    }
    if (!retry) {
        LOG_ERR("%s: IIC read 0x01 retry over times!!\n", __func__);
    }
/*
    if (mode == CAMERA_KEY_CAMERA_MODE) {
        cs_press_set_trigger_strength(g_cs_press.realtime_cfg[g_cs_press.cfg_chosen]);
    } else {
        cs_press_set_trigger_strength(g_cs_press.delay_cfg[g_cs_press.cfg_chosen]);
    }
*/
#endif
    LOG_INFO("exit %s\n", __func__);
    return ret;
}

#ifdef INT_SET_EN
/**
  * @brief    input system register
  * @param
  * @retval none
  */
void fml_input_dev_init(void){
    int ret = 0;

    cs_input_dev = input_allocate_device();
    if (cs_input_dev != NULL) {
        cs_input_dev->name = CS_PRESS_NAME;
        __set_bit(EV_KEY, cs_input_dev->evbit);
        __set_bit(KEY_HOME, cs_input_dev->keybit);
        __set_bit(KEY_VOLUMEUP, cs_input_dev->keybit);
        __set_bit(KEY_VOLUMEDOWN, cs_input_dev->keybit);
        __set_bit(KEY_POWER, cs_input_dev->keybit);
#ifdef CAMERA_KEY
        __set_bit(EV_SYN, cs_input_dev->evbit);
        __set_bit(EV_ABS, cs_input_dev->evbit);
        __set_bit(EV_KEY, cs_input_dev->evbit);
        __set_bit(KEY_LIGHT_TAP, cs_input_dev->keybit);
        __set_bit(KEY_HEAVY_TAP, cs_input_dev->keybit);
        __set_bit(KEY_SHORT_TAP, cs_input_dev->keybit);
        __set_bit(KEY_LONG_TAP, cs_input_dev->keybit);
        __set_bit(KEY_DOUBLE_TAP, cs_input_dev->keybit);
        __set_bit(KEY_SWIPE_UP, cs_input_dev->keybit);
        __set_bit(KEY_SWIPE_DOWN, cs_input_dev->keybit);
        __set_bit(KEY_PHYSICAL_TAP, cs_input_dev->keybit);
        input_set_abs_params(cs_input_dev, ABS_DISTANCE, -20, 20, 0, 0);
        input_set_abs_params(cs_input_dev, ABS_X, 0, 100, 0, 0);
#endif
        ret = input_register_device(cs_input_dev);
        if (ret != 0)
            LOG_ERR("input register device error = %d\n", ret);
    }
}

/**
  * @brief    input system unregister
  * @param
  * @retval none
  */
void fml_input_dev_exit(void){
    /* release input_dev */
    input_unregister_device(cs_input_dev);
    input_free_device(cs_input_dev);
}

/**
  * @brief    cs_irq_enable
  * @param
  * @retval
  */
static void cs_irq_enable(void)
{
    if (cs_irq_flag == 0) {
        cs_irq_flag++;
        enable_irq(cs_press_irq_num);
    } else {
        LOG_ERR("cs_press Eint already enabled!\n");
    }
    /*LOG_ERR("Enable irq_flag=%d\n", cs_irq_flag);*/
}

/**
  * @brief    cs_irq_disable
  * @param
  * @retval
*/
static void cs_irq_disable(void)
{
    if (cs_irq_flag == 1) {
        cs_irq_flag--;
        disable_irq_nosync(cs_press_irq_num);
    } else {
        LOG_ERR("cs_press Eint already disabled!\n");
    }
    /*LOG_ERR("Disable irq_flag=%d\n", cs_irq_flag);*/
}

/**
  * @brief    cs_press_interrupt_handler
  * @param
  * @retval
*/
static irqreturn_t cs_press_interrupt_handler(int irq, void *dev_id)
{
    cs_press_int_flag = 1;

    cs_irq_disable();
    if (g_cs_press.suspend_lock) {
        __pm_wakeup_event(g_cs_press.suspend_lock, 1000);
    }
    wake_up_interruptible(&cs_press_waiter);

    return IRQ_HANDLED;
}
/**
  * @brief    get_key_event
  * @param
  * @retval
*/
unsigned char get_key_event(void)
{
    unsigned char rbuf[1];
    unsigned char addr;
    int len = 0;

    addr = IIC_KEY_EVENT;
    len = 1;
    rbuf[0] = 0x0;
    if (cs_press_iic_read(addr, rbuf, len) <= 0) {
        LOG_ERR("reg=%d,buf[0]=%d,len=%d,err\n", addr, rbuf[0], len);
    }
    return rbuf[0];
}

#ifdef CAMERA_KEY
void report_camera_key(void)
{
    unsigned char rbuf[AP_FORCEDATA_LEN] = {0};
    unsigned char addr = AP_FORCEDATA_REG;
    int16_t distance, curr_pos, curr_force, start_pos, trig, rsts = 0;
    int16_t ch_rawdata[CH_COUNT] = { 0 };
    int16_t ch_baseline[CH_COUNT] = { 0 };
    int16_t ch_force[CH_COUNT] = { 0 };
    int32_t heavy_tap_lag[CH_COUNT] = { 0 };
    int i, j, j_max, ret = 0;
    int action = ACTION_UNKNOWN;
    int level = BIT_LEVEL_UNKNOWN;
    bool is_swipe = false;

    ret = cs_press_iic_read(addr, rbuf, AP_FORCEDATA_LEN);
    if (ret < 0) {
        LOG_ERR("read reg=0x%02x error, ret = %d\n", addr, ret);
        return;
    }

    if (rbuf[0] == 0xEE && rbuf[1] == 0xEE) {
        addr = DEBUG_RESET_SOURCE_REG;
        memset(rbuf, 0, AP_FORCEDATA_LEN);
        ret = cs_press_iic_read(addr, rbuf, 4);
        if (ret < 0) {
            LOG_ERR("read reg=0x%02x error, ret = %d\n", addr, ret);
        } else {
            LOG_ERR("IC reset[%02x %02x] happened, mode recovery...\n", rbuf[0], rbuf[1]);
            rsts = (int16_t)(rbuf[0] + (rbuf[1] << 8));
            if (rsts & RSTS_PIN) {
                g_cs_press.hard_reset_cnt++;
                LOG_DEBUG("[HARD_RESET]\n");
            }
            if (rsts & RSTS_SYS) {
                g_cs_press.soft_reset_cnt++;
                LOG_DEBUG("[SOFT_RESET]\n");
            }
            if ((rsts & RSTS_IWDG) || (rsts & RSTS_WDT)) {
                g_cs_press.wdt_reset_cnt++;
                LOG_DEBUG("[WDT_RESET]\n");
            }
            cs_press_clear_reset_register();
        }
        cs_press_set_mode(g_cs_press.camera_key_mode);
        return;
    }
    curr_pos = (int16_t)(rbuf[6] + (rbuf[7] << 8));
    curr_force = (int16_t)(rbuf[8] + (rbuf[9] << 8));
    distance = (int16_t)(rbuf[10] + (rbuf[11] << 8));
    start_pos = (int16_t)(rbuf[12] + (rbuf[13] << 8));
    ch_rawdata[0] = (int16_t)(rbuf[36] + (rbuf[37] << 8));
    ch_rawdata[1] = (int16_t)(rbuf[38] + (rbuf[39] << 8));
    ch_baseline[0] = (int16_t)(rbuf[40] + (rbuf[41] << 8));
    ch_baseline[1] = (int16_t)(rbuf[42] + (rbuf[43] << 8));
    ch_force[0] = (int16_t)(rbuf[44] + (rbuf[45] << 8));
    ch_force[1] = (int16_t)(rbuf[46] + (rbuf[47] << 8));
    trig = (int16_t)(rbuf[56] + (rbuf[57] << 8));
    LOG_DEBUG("mode=[%d], ation=[%02x %02x], area=[%02x %02x], level=[%02x %02x],"
            " curPos=%d, curForce=%d, swipeDis=%d, startPos=%d, trig=%d, modeType=[%02x %02x],"
            " chRaw=(%d %d), chBase=(%d %d), chForce=(%d %d)",
            g_cs_press.camera_key_mode, rbuf[0], rbuf[1], rbuf[2], rbuf[3], rbuf[4], rbuf[5],
            curr_pos, curr_force, distance, start_pos, trig, rbuf[14], rbuf[15],
            ch_rawdata[0], ch_rawdata[1], ch_baseline[0], ch_baseline[1], ch_force[0], ch_force[1]);

    if (distance < 0) {
        distance = -distance;
    }
    /*if (g_cs_press.mode_switch_waiting_up) {
        if (g_cs_press.is_long_tap_down && !(rbuf[1] & BIT_ACTION_LIFT_PHYSICAL_DOWN)
                && !(rbuf[0] & BIT_ACTION_PHYSICAL_TOUCHING)) {
            input_report_key(cs_input_dev, KEY_LONG_TAP, 0);
            input_sync(cs_input_dev);
            g_cs_press.is_long_tap_down = false;
        }
        if (!g_cs_press.is_long_tap_down) {
            cs_press_set_mode(g_cs_press.camera_key_mode);
            g_cs_press.mode_switch_waiting_up = false;
        }
    } else */
    if (g_cs_press.camera_key_mode == CAMERA_KEY_CAMERA_MODE) {
        //LOG_DEBUG("in REALTIME_EVENT_MODE\n");
        /* Tap Event */
        if (rbuf[0] & BIT_ACTION_TAP_DOWN) {
            action = ACTION_DOWN;
        } else if (rbuf[0] & BIT_ACTION_TAP_UP) {
            action = ACTION_UP;
        }
        if (rbuf[4]) {
            level = rbuf[4];
        }

        if (((rbuf[1] & BIT_ACTION_LIFT_PHYSICAL_DOWN)
                || (rbuf[0] & BIT_ACTION_PHYSICAL_TOUCHING)) && !g_cs_press.is_physical_tap_down) {
            LOG_INFO("[REPORT_KEY]KEY_PHYSICAL_TAP down\n");
            input_report_key(cs_input_dev, KEY_PHYSICAL_TAP, 1);
            input_sync(cs_input_dev);
            g_cs_press.is_physical_tap_down = true;
        }
        if (action != ACTION_UNKNOWN
                && level != BIT_LEVEL_UNKNOWN) {
            if (action) {
                if ((level & BIT_LEVEL_TOUCH) && !g_cs_press.is_physical_tap_down) {
                    LOG_INFO("[REPORT_KEY]KEY_PHYSICAL_TAP down\n");
                    input_report_key(cs_input_dev, KEY_PHYSICAL_TAP, 1);
                    input_sync(cs_input_dev);
                    g_cs_press.is_physical_tap_down = true;
                }
                if ((level & BIT_LEVEL_LIGHT) && !g_cs_press.is_light_tap_down) {
                    LOG_INFO("[REPORT_KEY]KEY_LIGHT_TAP down\n");
                    input_report_key(cs_input_dev, KEY_LIGHT_TAP, 1);
                    input_sync(cs_input_dev);
                    g_cs_press.is_light_tap_down = true;
                }
                if ((level & BIT_LEVEL_HEAVY) && !g_cs_press.is_heavy_tap_down) {
                    LOG_INFO("[REPORT_KEY]KEY_HEAVY_TAP down\n");
                    input_report_key(cs_input_dev, KEY_HEAVY_TAP, 1);
                    input_sync(cs_input_dev);
                    g_cs_press.is_heavy_tap_down = true;
                }
            } else {
                if ((level & BIT_LEVEL_HEAVY) && g_cs_press.is_heavy_tap_down) {
                    LOG_INFO("[REPORT_KEY]KEY_HEAVY_TAP up\n");
                    input_report_key(cs_input_dev, KEY_HEAVY_TAP, 0);
                    input_sync(cs_input_dev);
                    g_cs_press.is_heavy_tap_down = false;
                    for (i = 0; i < CH_COUNT; i++) {
                        if (g_cs_press.heavy_tap_force_max[i]) {
                            j_max = 0;
                            heavy_tap_lag[i] = ch_force[i] * 1000 / g_cs_press.heavy_tap_force_max[i];
                            for (j = 0; j < LAG_MIN_COUNT; j++) {
                                if (!g_cs_press.heavy_tap_lag[i][j]) {
                                    j_max = j;
                                    break;
                                } else if (g_cs_press.heavy_tap_lag[i][j] > g_cs_press.heavy_tap_lag[i][j_max]) {
                                    j_max = j;
                                }
                            }
                            if (!g_cs_press.heavy_tap_lag[i][j_max]
                                    || (g_cs_press.heavy_tap_lag[i][j_max] > heavy_tap_lag[i])) {
                                g_cs_press.heavy_tap_lag[i][j_max] = heavy_tap_lag[i];
                            }
                        }
                        g_cs_press.heavy_tap_force_max[i] = 0;
                    }
                    LOG_INFO("heavy_tap_lag=[%d, %d]\n", heavy_tap_lag[0], heavy_tap_lag[1]);
                }
                if ((level & BIT_LEVEL_LIGHT) && g_cs_press.is_light_tap_down) {
                    LOG_INFO("[REPORT_KEY]KEY_LIGHT_TAP up\n");
                    input_report_key(cs_input_dev, KEY_LIGHT_TAP, 0);
                    input_sync(cs_input_dev);
                    g_cs_press.is_light_tap_down = false;
                }
            }
        }
        /* Swipe Event */
        if (rbuf[1] & BIT_ACTION_FOLLOW_SWIPE_UP) {
            LOG_INFO("[REPORT_KEY]KEY_SWIPE_UP %d\n", distance);
            input_report_key(cs_input_dev, KEY_SWIPE_UP, 1);
            input_report_abs(cs_input_dev, ABS_DISTANCE, distance);
            input_report_abs(cs_input_dev, ABS_X, curr_pos);
            input_sync(cs_input_dev);
            input_report_key(cs_input_dev, KEY_SWIPE_UP, 0);
            input_report_abs(cs_input_dev, ABS_DISTANCE, 0);
            input_report_abs(cs_input_dev, ABS_X, 0);
            input_sync(cs_input_dev);
            is_swipe = true;
        } else if (rbuf[1] & BIT_ACTION_FOLLOW_SWIPE_DOWN) {
            LOG_INFO("[REPORT_KEY]KEY_SWIPE_DOWN %d\n", distance);
            input_report_key(cs_input_dev, KEY_SWIPE_DOWN, 1);
            input_report_abs(cs_input_dev, ABS_DISTANCE, distance);
            input_report_abs(cs_input_dev, ABS_X, curr_pos);
            input_sync(cs_input_dev);
            input_report_key(cs_input_dev, KEY_SWIPE_DOWN, 0);
            input_report_abs(cs_input_dev, ABS_DISTANCE, 0);
            input_report_abs(cs_input_dev, ABS_X, 0);
            input_sync(cs_input_dev);
            is_swipe = true;
        }
        if (g_cs_press.is_physical_tap_down && (((action == ACTION_UP) && (level & BIT_LEVEL_TOUCH))
                    || (!(rbuf[1] & BIT_ACTION_LIFT_PHYSICAL_DOWN) && !(rbuf[0] & BIT_ACTION_PHYSICAL_TOUCHING)))) {
            LOG_INFO("[REPORT_KEY]KEY_PHYSICAL_TAP up\n");
            input_report_key(cs_input_dev, KEY_PHYSICAL_TAP, 0);
            input_sync(cs_input_dev);
            g_cs_press.is_physical_tap_down = false;
        }
        if (g_cs_press.is_light_tap_down || g_cs_press.is_heavy_tap_down) {
            if (!g_cs_press.tap_force_min || (curr_force < g_cs_press.tap_force_min)) {
                g_cs_press.tap_force_min = curr_force;
            }
            if (!g_cs_press.tap_force_max || (curr_force > g_cs_press.tap_force_max)) {
                g_cs_press.tap_force_max = curr_force;
            }
        }
        if (g_cs_press.is_heavy_tap_down) {
            for (i = 0; i < CH_COUNT; i++) {
                if (ch_force[i] > g_cs_press.heavy_tap_force_max[i]) {
                    g_cs_press.heavy_tap_force_max[i] = ch_force[i];
                }
            }
        }

        if (is_swipe) {
            if (!g_cs_press.swipe_force_min || (curr_force < g_cs_press.swipe_force_min)) {
                g_cs_press.swipe_force_min = curr_force;
            }
            if (!g_cs_press.swipe_force_max || (curr_force > g_cs_press.swipe_force_max)) {
                g_cs_press.swipe_force_max = curr_force;
            }
        }
    } else {
        //LOG_DEBUG("in DELAY_EVENT_MODE\n");
        if (((rbuf[1] & BIT_ACTION_LIFT_PHYSICAL_DOWN)
                || (rbuf[0] & BIT_ACTION_PHYSICAL_TOUCHING)) && !g_cs_press.is_physical_tap_down) {
            LOG_INFO("[REPORT_KEY]KEY_PHYSICAL_TAP down(ignore)\n");
            g_cs_press.is_physical_tap_down = true;
        } else if (!(rbuf[1] & BIT_ACTION_LIFT_PHYSICAL_DOWN)
                && !(rbuf[0] & BIT_ACTION_PHYSICAL_TOUCHING)
                && g_cs_press.is_physical_tap_down) {
            LOG_INFO("[REPORT_KEY]KEY_PHYSICAL_TAP up(ignore)\n");
            g_cs_press.is_physical_tap_down = false;
        }
        /* Tap Event */
        if (rbuf[0] & BIT_ACTION_DOUBLE_TAP) {
            LOG_INFO("[REPORT_KEY]KEY_DOUBLE_TAP\n");
            input_report_key(cs_input_dev, KEY_DOUBLE_TAP, 1);
            input_sync(cs_input_dev);
            input_report_key(cs_input_dev, KEY_DOUBLE_TAP, 0);
            input_sync(cs_input_dev);
            g_cs_press.double_tap_cnt++;
        } else if (rbuf[0] & BIT_ACTION_SHORT_TAP) {
            LOG_INFO("[REPORT_KEY]KEY_SHORT_TAP\n");
            input_report_key(cs_input_dev, KEY_SHORT_TAP, 1);
            input_sync(cs_input_dev);
            input_report_key(cs_input_dev, KEY_SHORT_TAP, 0);
            input_sync(cs_input_dev);
        }/* else if (rbuf[0] & BIT_ACTION_LONG_TAP) {
            LOG_INFO("[REPORT_KEY]KEY_LONG_TAP\n");
            input_report_key(cs_input_dev, KEY_LONG_TAP, 1);
            input_sync(cs_input_dev);
            if ((rbuf[1] & BIT_ACTION_LIFT_PHYSICAL_DOWN)
                        || (rbuf[0] & BIT_ACTION_PHYSICAL_TOUCHING)) {
                g_cs_press.is_long_tap_down = true;
            } else {
            input_report_key(cs_input_dev, KEY_LONG_TAP, 0);
            input_sync(cs_input_dev);
            }
        }*/
        /*if (g_cs_press.is_long_tap_down
                && !(rbuf[1] & BIT_ACTION_LIFT_PHYSICAL_DOWN)
                && !(rbuf[0] & BIT_ACTION_PHYSICAL_TOUCHING)) {
            input_report_key(cs_input_dev, KEY_LONG_TAP, 0);
            input_sync(cs_input_dev);
            g_cs_press.is_long_tap_down = false;
        }*/
        /* Swipe Event */
        if (rbuf[1] & BIT_ACTION_LIFT_SWIPE_UP) {
            LOG_INFO("[REPORT_KEY]KEY_SWIPE_UP %d\n", distance);
            input_report_key(cs_input_dev, KEY_SWIPE_UP, 1);
            input_report_abs(cs_input_dev, ABS_DISTANCE, distance);
            input_report_abs(cs_input_dev, ABS_X, curr_pos);
            input_sync(cs_input_dev);
            input_report_key(cs_input_dev, KEY_SWIPE_UP, 0);
            input_report_abs(cs_input_dev, ABS_DISTANCE, 0);
            input_report_abs(cs_input_dev, ABS_X, 0);
            input_sync(cs_input_dev);
        } else if (rbuf[1] & BIT_ACTION_LIFT_SWIPE_DOWN) {
            LOG_INFO("[REPORT_KEY]KEY_SWIPE_DOWN %d\n", distance);
            input_report_key(cs_input_dev, KEY_SWIPE_DOWN, 1);
            input_report_abs(cs_input_dev, ABS_DISTANCE, distance);
            input_report_abs(cs_input_dev, ABS_X, curr_pos);
            input_sync(cs_input_dev);
            input_report_key(cs_input_dev, KEY_SWIPE_DOWN, 0);
            input_report_abs(cs_input_dev, ABS_DISTANCE, 0);
            input_report_abs(cs_input_dev, ABS_X, 0);
            input_sync(cs_input_dev);
        }
    }
}
#endif

/**
  * @brief    key event  handler
  * @param
  * @retval
*/
void fml_key_report(void)
{
#ifdef SIDE_KEY
    unsigned char event = 0;
    event = get_key_event();
    if((event & 0x01) == 0x01){
        LOG_ERR("vol down key.\n");
        input_report_key(cs_input_dev, KEY_VOLUMEDOWN, 1);
        input_sync(cs_input_dev);
        input_report_key(cs_input_dev, KEY_VOLUMEDOWN, 0);
        input_sync(cs_input_dev);
    }else if((event & 0x02) == 0x02){
        LOG_ERR("vol up key.\n");
        input_report_key(cs_input_dev, KEY_VOLUMEUP, 1);
        input_sync(cs_input_dev);
        input_report_key(cs_input_dev, KEY_VOLUMEUP, 0);
        input_sync(cs_input_dev);
    }else if((event & 0x04) == 0x04){
        LOG_ERR("power key .\n");
        input_report_key(cs_input_dev, KEY_POWER, 1);
        input_sync(cs_input_dev);
        input_report_key(cs_input_dev, KEY_POWER, 0);
        input_sync(cs_input_dev);
    }
#endif

#ifdef HOME_KEY
    if(gpio_get_value(cs_press_irq_gpio) == 0){
        LOG_ERR("Home key down.\n");
        input_report_key(cs_input_dev, KEY_HOME, 1);
        input_sync(cs_input_dev);

    }else if(gpio_get_value(cs_press_irq_gpio) == 1){
        LOG_ERR("Home key up.\n");
        input_report_key(cs_input_dev, KEY_HOME, 0);
        input_sync(cs_input_dev);
    }
#endif

#ifdef CAMERA_KEY
    report_camera_key();
#endif
}

/**
  * @brief    cs_press_event_handler
  * @param
  * @retval
*/
static int cs_press_event_handler(void *unused)
{
    do {
        wait_event_interruptible(cs_press_waiter,
            cs_press_int_flag != 0);
        mutex_lock(&press_lock);
        cs_press_int_flag = 0;

        fml_key_report();
        cs_irq_enable();
        mutex_unlock(&press_lock);
    } while (!kthread_should_stop());

    return 0;
}

/**
  * @brief
  * @param
  * @retval
*/
void eint_init(void)
{
    init_waitqueue_head(&cs_press_waiter);

    kthread_run(cs_press_event_handler, 0, CS_PRESS_NAME);
    cs_irq_enable();
    LOG_ERR("init_irq ok");
    fml_input_dev_init();
}
void eint_exit(void)
{
    fml_input_dev_exit();
}
#endif
/**
  * @brief  clean debug mode reg, debug ready reg
  * @param  None
  * @retval 0:success, -1: fail
  */
static char cs_press_clean_debugmode(void)
{
    char ret = 0;
    unsigned char temp_data = 0;

    ret = cs_press_iic_write(DEBUG_MODE_REG, &temp_data, 1);
    if(ret < 0){
        LOG_ERR("DEBUG_MODE_REG write failed\n");
        return -1;
    }
    ret = cs_press_iic_write(DEBUG_READY_REG, &temp_data, 1);
    if(ret < 0){
        LOG_ERR("DEBUG_READY_REG write failed\n");
        return -1;
    }
    return 0;
}

/**
  * @brief  set debug mode reg
  * @param  mode_num: debug mode num data
  * @retval 0:success, -1: fail
  */
static char cs_press_set_debugmode(unsigned char mode_num)
{
    int ret = 0;

    ret = cs_press_iic_write(DEBUG_MODE_REG, &mode_num, 1);
    if(ret < 0){
        LOG_ERR("DEBUG_MODE_REG write failed\n");
        return -1;
    }
    return 0;
}

/**
  * @brief  set debug ready reg
  * @param  ready_num: debug ready num data
  * @retval 0:success, -1: fail
  */
static char cs_press_set_debugready(unsigned char ready_num)
{
    int ret = 0;

    ret = cs_press_iic_write(DEBUG_READY_REG, &ready_num, 1);
    if(ret < 0){
        LOG_ERR("DEBUG_READY_REG write failed\n");
        return -1;
    }
    return 0;
}

/**
  * @brief  get debug ready reg data
  * @param  None
  * @retval ready reg data
  */
static unsigned char cs_press_get_debugready(void)
{
    int ret = 0;
    unsigned char ready_num = 0;

    ret = cs_press_iic_read(DEBUG_READY_REG, &ready_num, 1);
    if(ret < 0)
    {
        ready_num = 0;
    }
    return ready_num;
}

/**
  * @brief      write debug data
  * @param[in]  *debugdata: point to data buffer,
  * @param[in]  length:     write data length
  * @retval     0:success, -1: fail
  */
static char cs_press_write_debugdata(unsigned char *debugdata, unsigned char length)
{
    int ret = 0;

    ret = cs_press_iic_write(DEBUG_DATA_REG, debugdata, length);
    if(ret < 0){
        LOG_ERR("DEBUG_DATA_REG write failed\n");
        return (char)-1;
    }
    return 0;
}

/**
  * @brief  read debug data
  * @param  *debugdata: point to data buffer, length: write data length
  * @retval 0:success, -1: fail
  */
static char cs_press_read_debugdata(unsigned char *debugdata, unsigned char length)
{
    int ret = 0;

    if(debugdata == NULL){
        LOG_ERR("ERR:buf is NULL\n");
        return (char)-1;
    }
    if(length == 0){
        LOG_ERR("ERR:read lenth = 0\n");
        return (char)-1;
    }
    ret = cs_press_iic_read(DEBUG_DATA_REG, debugdata, length);
    if(ret < 0){
        LOG_ERR("DEBUG_DATA_REG read failed\n");
        return (char)-1;
    }
    return 0;
}

static char cs_press_gpio_toggle(unsigned char new_state)
{
    int ret = 0;
    unsigned char retry = RETRY_NUM;
    unsigned char temp_data[2] = {0x86,0x7A};/*enable gpio_toggle*/
    if(new_state == 0) /*disable gpio_toggle*/
    {
        temp_data[0] = 0x87;
        temp_data[1] = 0x79;
    }
    do
    {
        ret = cs_press_iic_write(0x02, temp_data, 2);
    }while((ret != 0 ) && (retry--));
    return ret;
}

/**
  * @brief  clear reset register
  * @param  None
  * @retval 0:success, -1: fail
  */
char cs_press_clear_reset_register(void)
{
    char ret = 0;
    unsigned char retry = RETRY_NUM;
    // M series required write value 0xcc
    unsigned char temp_data[2] = {0xB7, 0x49};
    unsigned char addr = DEBUG_RESET_SOURCE_REG;
    unsigned char rbuf[4] = {0};

    LOG_INFO("%s\n", __func__);
    // repeatly reset including first time i2c wake up
    do
    {
        if(ret!=0)
        {
            cs_press_delay_ms(1);
        }
        ret = cs_press_iic_write(AP_RESET_MCU_REG, temp_data, 2);
    }while((ret != 0) && (retry--));

    cs_press_delay_ms(5);
    ret = cs_press_iic_read(addr, rbuf, 4);
    if (ret >= 0) {
        LOG_DEBUG("after clear, reset register read [%02x %02x].\n", rbuf[0], rbuf[1]);
    }

    return ret;
}


/**
  * @brief  watchdog reset the device
  * @param  None
  * @retval 0:success, -1: fail
  */
char cs_press_wdt_reset_device(void)
{
    char ret = 0;
    unsigned char retry = RETRY_NUM;
    // M series required write value 0xcc
    unsigned char temp_data[2] = {0xB8, 0x48};

    LOG_INFO("%s\n", __func__);
    // repeatly reset including first time i2c wake up
    do
    {
        if(ret!=0)
        {
            cs_press_delay_ms(1);
        }
        ret = cs_press_iic_write(AP_RESET_MCU_REG, temp_data, 2);
    }while((ret != 0) && (retry--));

    return ret;
}

/**
  * @brief  soft_reset the device
  * @param  None
  * @retval 0:success, -1: fail
  */
char cs_press_soft_reset_device(void)
{
    char ret = 0;
    unsigned char retry = RETRY_NUM;
    // M series required write value 0xcc
    unsigned char temp_data[2] = {0xB5, 0x4B};

    LOG_INFO("%s\n", __func__);
    // repeatly reset including first time i2c wake up
    do
    {
        if(ret!=0)
        {
            cs_press_delay_ms(1);
        }
        ret = cs_press_iic_write(AP_RESET_MCU_REG, temp_data, 2);
    }while((ret != 0) && (retry--));

    return ret;
}

/**
  * @brief  reset ic
  * @param  None
  * @retval 0:success, -1: fail
  */
char cs_press_reset_ic(void)
{
    char ret = 0;

    LOG_ERR("enter cs_press_reset_ic\n");
    rt_mutex_lock(&(g_cs_press.client->adapter->bus_lock));
    #if RSTPIN_RESET_ENABLE
        cs_press_rstpin_high();
        cs_press_delay_ms(100);
        cs_press_rstpin_low();
        cs_press_delay_ms(80);

    #else/* hw reset ic*/
        cs_press_power_down();
        cs_press_delay_ms(100);
        cs_press_power_up();
        cs_press_delay_ms(80);

    #endif
    rt_mutex_unlock(&(g_cs_press.client->adapter->bus_lock));
    LOG_ERR("exit cs_press_reset_ic\n");
    return ret;
}

#ifdef CAMERA_KEY
/**
  * @brief  init config para
  * @param  None
  * @retval 0:success, -1: fail
  */
int camera_key_para_init(void)
{
    int rc;
    int i, j;
    int ret = 0;
    struct device_node *np;
    char cfg_name[CAMERA_KEY_CFG_NAME_LEN] = { 0 };
    uint32_t cfg_param[STRENGTH_CFG_NUM] = { 0 };
    trigger_strength_config *cfg;

    np = g_cs_press.client->dev.of_node;

    for (i = 0; i < CAMERA_KEY_CFG_MAX; i++) {
        for (j = 0; j < 2; j++) {
            memset(cfg_name, 0, CAMERA_KEY_CFG_NAME_LEN);
            memset(cfg_param, 0, STRENGTH_CFG_NUM);
            if (!j) {
                snprintf(cfg_name, CAMERA_KEY_CFG_NAME_LEN, "realtime-mode-%d", i);
                cfg = &g_cs_press.realtime_cfg[i];
            } else {
                snprintf(cfg_name, CAMERA_KEY_CFG_NAME_LEN, "delay-mode-%d", i);
                cfg = &g_cs_press.delay_cfg[i];
            }
            rc = of_property_read_u32_array(np, cfg_name, cfg_param, STRENGTH_CFG_NUM);
            if (rc < 0) {
                memcpy(cfg, &g_cs_press.strength_cfg, sizeof(trigger_strength_config));
            } else {
                cfg->light_tap_down_strength = cfg_param[0] / STRENGTH_PER_LEVEL;
                cfg->light_tap_up_strength = cfg_param[1] / STRENGTH_PER_LEVEL;
                cfg->heavy_tap_down_strength = cfg_param[2] / STRENGTH_PER_LEVEL;
                cfg->heavy_tap_up_strength = cfg_param[3] / STRENGTH_PER_LEVEL;
                cfg->light_swipe_down_strength = cfg_param[4] / STRENGTH_PER_LEVEL;
                cfg->light_swipe_up_strength = cfg_param[5] / STRENGTH_PER_LEVEL;
                cfg->heavy_swipe_down_strength = cfg_param[6] / STRENGTH_PER_LEVEL;
                cfg->heavy_swipe_up_strength = cfg_param[7] / STRENGTH_PER_LEVEL;
                cfg->area_1 = cfg_param[8];
                cfg->area_2 = cfg_param[9];
                cfg->long_tap_judge_time = cfg_param[10] / TIME_MS_PER_LEVEL;
                cfg->muti_tap_judge_time = cfg_param[11] / TIME_MS_PER_LEVEL;
            }
            LOG_INFO("%s %s: tap_strength[%u,%u,%u,%u], swipe_strength[%u,%u,%u,%u], area[%u,%u], judge_time[%u,%u]\n",
                cfg_name, (rc < 0) ? "not set, default config" : "set config",
                cfg->light_tap_down_strength, cfg->light_tap_up_strength,
                cfg->heavy_tap_down_strength, cfg->heavy_tap_up_strength,
                cfg->light_swipe_down_strength, cfg->light_swipe_up_strength,
                cfg->heavy_swipe_down_strength, cfg->heavy_swipe_up_strength,
                cfg->area_1, cfg->area_2, cfg->long_tap_judge_time, cfg->muti_tap_judge_time);
        }
    }

    return ret;
}

int camera_key_config_read(trigger_strength_config *strength_cfg)
{
    unsigned char cfg_read[STRENGTH_CFG_NUM] = {0};
    int ret = 0;

    ret = cs_press_iic_read(AP_RW_SCANT_PERIOD_REG, cfg_read, STRENGTH_CFG_NUM);
    if (ret >= 0) {
        strength_cfg->light_tap_down_strength = cfg_read[0];
        strength_cfg->light_tap_up_strength = cfg_read[1];
        strength_cfg->heavy_tap_down_strength = cfg_read[2];
        strength_cfg->heavy_tap_up_strength = cfg_read[3];
        strength_cfg->light_swipe_down_strength = cfg_read[4];
        strength_cfg->light_swipe_up_strength = cfg_read[5];
        strength_cfg->heavy_swipe_down_strength = cfg_read[6];
        strength_cfg->heavy_swipe_up_strength = cfg_read[7];
        strength_cfg->area_1 = cfg_read[8];
        strength_cfg->area_2 = cfg_read[9];
        strength_cfg->long_tap_judge_time = cfg_read[10];
        strength_cfg->muti_tap_judge_time = cfg_read[11];
        LOG_INFO("trigger config: tap_strength[%u,%u,%u,%u], swipe_strength[%u,%u,%u,%u], area[%u,%u], judge_time[%u,%u]\n",
                strength_cfg->light_tap_down_strength, strength_cfg->light_tap_up_strength,
                strength_cfg->heavy_tap_down_strength, strength_cfg->heavy_tap_up_strength,
                strength_cfg->light_swipe_down_strength, strength_cfg->light_swipe_up_strength,
                strength_cfg->heavy_swipe_down_strength, strength_cfg->heavy_swipe_up_strength,
                strength_cfg->area_1, strength_cfg->area_2,
                strength_cfg->long_tap_judge_time, strength_cfg->muti_tap_judge_time);
    }
    return ret;
}
#endif

int cs_press_get_offset(int32_t *offset)
{
    unsigned char rbuf[CH_COUNT * 2] = {0};
    int16_t dac_conver = 0;
    int i, ret = 0;

    ret = cs_press_iic_read(AP_DAC_UV_CONVER_REG, rbuf, 2);
    if (ret < 0) {
        LOG_ERR("read reg=0x%02x error, ret = %d\n", AP_OFFSET_REG, ret);
        return -1;
    }
    dac_conver = (int16_t)(rbuf[0] + (rbuf[1] << 8));

    memset(rbuf, 0, CH_COUNT * 2);
    ret = cs_press_iic_read(AP_OFFSET_REG, rbuf, CH_COUNT * 2);
    if (ret < 0) {
        LOG_ERR("read reg=0x%02x error, ret = %d\n", AP_OFFSET_REG, ret);
        return -1;
    }
    for (i = 0; i < CH_COUNT; i++) {
        offset[i] = (int16_t)(rbuf[2 * i] + (rbuf[2 * i + 1] << 8)) * dac_conver;
        LOG_INFO("offset[%i] = %d (%d[%02x %02x] * %d)\n", i, offset[i],
                (int16_t)(rbuf[2 * i] + (rbuf[2 * i + 1] << 8)), rbuf[2 * i], rbuf[2 * i + 1], dac_conver);
    }

    return ret;
}


int cs_press_get_noise_var(int32_t *noise_std)
{
    unsigned char rbuf[AP_FORCEDATA_LEN] = {0};
    int16_t dac_conver = 0;
    int i, ret = 0;
    int retry_times = 5;
    int16_t pre_adc[CH_COUNT] = {0};
    int16_t adc_delta[CH_COUNT][NOISE_TEST_COUNT] = {0};
    int32_t sum[CH_COUNT] = {0};
    int32_t avg[CH_COUNT] = {0};

    ret = cs_press_iic_read(AP_DAC_UV_CONVER_REG, rbuf, 2);
    if (ret < 0) {
        LOG_ERR("read reg=0x%02x error, ret = %d\n", AP_OFFSET_REG, ret);
        return -1;
    }
    dac_conver = (int16_t)(rbuf[0] + (rbuf[1] << 8));
retry:
    for (i = 0; i < NOISE_TEST_COUNT + 1; i++) {
        ret = cs_press_iic_read(AP_FORCEDATA_REG, rbuf, AP_FORCEDATA_LEN);
        if (ret < 0) {
            LOG_ERR("read reg=0x%02x error, ret = %d\n", AP_FORCEDATA_REG, ret);
            if (retry_times) {
                retry_times--;
                msleep(20);
                goto retry;
            }
            return -1;
        }
        if (rbuf[36] == 0xEE && rbuf[37] == 0xEE) {
            if (retry_times) {
                LOG_ERR("read reg=0x%02x 0xEE\n", AP_FORCEDATA_REG);
                retry_times--;
                msleep(20);
                goto retry;
            } else {
                noise_std[0] = 0xEE;
                noise_std[1] = 0xEE;
                LOG_INFO("noise_std0=%d, noise_std1=%d", noise_std[0], noise_std[1]);
                return -1;
            }
        }
        if (!i) {
            pre_adc[0] = (int16_t)(rbuf[36] + (rbuf[37] << 8));
            pre_adc[1] = (int16_t)(rbuf[38] + (rbuf[39] << 8));
        } else {
            adc_delta[0][i - 1] = (int16_t)(rbuf[36] + (rbuf[37] << 8)) - pre_adc[0];
            adc_delta[1][i - 1] = (int16_t)(rbuf[38] + (rbuf[39] << 8)) - pre_adc[1];
            pre_adc[0] = (int16_t)(rbuf[36] + (rbuf[37] << 8));
            pre_adc[1] = (int16_t)(rbuf[38] + (rbuf[39] << 8));
            sum[0] += adc_delta[0][i - 1];
            sum[1] += adc_delta[1][i - 1];
            LOG_INFO("%d:adc_delta[0]=%d[%02x %02x](sum=%d), adc_delta[1]=%d[%02x %02x](sum=%d)",
                        i, adc_delta[0][i - 1], rbuf[36], rbuf[37], sum[0], adc_delta[1][i - 1], rbuf[38], rbuf[39], sum[1]);
        }
    }
    avg[0] = sum[0] / NOISE_TEST_COUNT;
    avg[1] = sum[1] / NOISE_TEST_COUNT;

    sum[0] = 0;
    sum[1] = 0;
    for (i = 0; i < NOISE_TEST_COUNT; i++) {
        sum[0] += (adc_delta[0][i] - avg[0]) * (adc_delta[0][i] - avg[0]);
        sum[1] += (adc_delta[1][i] - avg[1]) * (adc_delta[1][i] - avg[1]);
        LOG_INFO("%d:var0=%d, var1=%d", i, sum[0], sum[1]);
    }
    noise_std[0] = sum[0] * dac_conver * dac_conver / NOISE_TEST_COUNT;
    noise_std[1] = sum[1] * dac_conver * dac_conver / NOISE_TEST_COUNT;
    LOG_INFO("noise_std0=%d, noise_std1=%d", noise_std[0], noise_std[1]);

    return ret;
}

void set_device_updating_flag(unsigned char val)
{
    g_cs_press.updating_flag = val;
}

unsigned char  get_device_updating_flag(void)
{
    return g_cs_press.updating_flag;
}
/**
  * @brief      forced firmware update
  * @param[in]  *fw_array: point to fw hex array
  * @retval     0:success, -1: fail
  */
char cs_press_fw_force_update(const unsigned char *fw_array)
{
    unsigned int i = 0;
    unsigned int j = 0;
    int ret = 0;
    char result = 0;
    unsigned int fw_code_length = 0;
    unsigned int fw_block_num_w = 0;
    unsigned int fw_block_num_r = 0;
    const unsigned char *fw_code_start = NULL;
    unsigned int fw_count = 0;
    unsigned char fw_read_code[FW_ONE_BLOCK_LENGTH_R] = {0};
    unsigned short fw_default_version = 0;
    unsigned short fw_read_version = 0;
    unsigned char boot_fw_write_cmd[BOOT_CMD_LENGTH] = BOOT_FW_WRITE_CMD;
    unsigned char boot_fw_wflag_cmd[BOOT_CMD_LENGTH] = BOOT_FW_WFLAG_CMD;
    unsigned char page_end = 0;

#ifdef INT_SET_EN
    cs_irq_disable(); /*close enit irq.*/
#endif
    /* fw init */
    fw_code_length = ((((unsigned short)fw_array[FW_ADDR_CODE_LENGTH+0]<<8)&0xff00)|fw_array[FW_ADDR_CODE_LENGTH+1]);
    fw_code_start = &fw_array[FW_ADDR_CODE_START];
    fw_block_num_w = fw_code_length/FW_ONE_BLOCK_LENGTH_W;
    fw_block_num_r = fw_code_length/FW_ONE_BLOCK_LENGTH_R;
    fw_default_version = ((((unsigned short)fw_array[FW_ADDR_VERSION+0]<<8)&0xff00)|fw_array[FW_ADDR_VERSION+1]);

    if(fw_code_length % 128){
        LOG_INFO("fw is not 128*\n");
        goto FLAG_FW_FAIL;
    }
    page_end = fw_code_length % 256;
    cs_press_reset_ic();
    /* send fw write cmd */
    cs_press_iic_write_double_reg(BOOT_CMD_REG ,boot_fw_write_cmd, BOOT_CMD_LENGTH);
    cs_press_delay_ms(1500);    /* waiting flash erase*/
    /* send fw code */
    fw_count = 0;
    for(i=0;i<fw_block_num_w;i++)
    {
        ret = cs_press_iic_write_double_reg(i*FW_ONE_BLOCK_LENGTH_W, (unsigned char*)fw_code_start+fw_count, FW_ONE_BLOCK_LENGTH_W);
        fw_count += FW_ONE_BLOCK_LENGTH_W;
        if(ret < 0)
        {
            LOG_ERR("ERR:iic write failed\n");
            result = (char)-1;
            goto FLAG_FW_FAIL;
        }
        cs_press_delay_ms(10);
    }
    /* read & check fw code */
    fw_count = 0;
    for(i = 0; i < fw_block_num_r; i++)
    {
        /* read code data */
        ret = cs_press_iic_read_double_reg(i*FW_ONE_BLOCK_LENGTH_R, fw_read_code, FW_ONE_BLOCK_LENGTH_R);
        if(ret < 0)
        {
            LOG_ERR("ERR:iic write failed\n");
            result = (char)-1;
            goto FLAG_FW_FAIL;
        }
        /* check code data */
        for(j = 0; j < FW_ONE_BLOCK_LENGTH_R; j++)
        {
            if(fw_read_code[j] != fw_code_start[fw_count+j])
            {
                LOG_ERR("ERR:check code data failed\n");
                result = (char)-1;
                goto FLAG_FW_FAIL;
            }
        }
        fw_count += FW_ONE_BLOCK_LENGTH_R;
        cs_press_delay_ms(5);
    }
    if(page_end > 0){
        /* read code data */
        ret = cs_press_iic_read_double_reg(fw_block_num_r*FW_ONE_BLOCK_LENGTH_R, fw_read_code, 128);
        if(ret < 0)
        {
            LOG_ERR("ERR:iic write failed\n");
            result = (char)-1;
            goto FLAG_FW_FAIL;
        }
        /* check code data */
        for(j = 0; j < 128; j++)
        {
            if(fw_read_code[j] != fw_code_start[fw_count+j])
            {
                LOG_ERR("ERR:check code data failed\n");
                result = (char)-1;
                goto FLAG_FW_FAIL;
            }
        }
        cs_press_delay_ms(5);
    }

    /* send fw flag cmd */
    cs_press_iic_write_double_reg(BOOT_CMD_REG ,boot_fw_wflag_cmd, BOOT_CMD_LENGTH);
    cs_press_delay_ms(50);
    /* reset */
    cs_press_reset_ic();
    /* check fw version */
    cs_press_delay_ms(300); /* skip boot */
    ret = cs_press_iic_read(AP_VERSION_REG, fw_read_code, CS_FW_VERSION_LENGTH);
    fw_read_version = 0;

    if(ret >= 0){
        fw_read_version = ((((unsigned short)fw_read_code[2]<<8)&0xff00)|fw_read_code[3]);
    }
    LOG_INFO("bin ver:%d.%d,soc ver:%d.%d\n", (fw_default_version>>8),(fw_default_version&0xff),\
                                                 (fw_read_version>>8),(fw_read_version&0xff));
    if(fw_read_version != fw_default_version){
        LOG_ERR("ERR:fw_read_version != fw_default_version\n");
        result = (char)-1;
        goto FLAG_FW_FAIL;
    }
FLAG_FW_FAIL:
#ifdef INT_SET_EN
    cs_irq_enable(); /*open enit irq.*/
#endif
    return result;
}


/**
  * @brief      firmware high version update
  * @param[in]  *fw_array: point to fw hex array
  * @retval     0:success, -1: fail, 1: no need update
  */
char cs_press_fw_high_version_update(const unsigned char *fw_array)
{
    int ret = 0;
    char result = 0;
    unsigned char read_temp[FW_ONE_BLOCK_LENGTH_R] = {0};
    unsigned short read_version = 0;
    unsigned short default_version = 0;
    char flag_update = 0;   /* 0: no need update fw, 1: need update fw */
    unsigned char retry = RETRY_NUM;

    cs_press_delay_ms(300); /* skip boot jump time */
    /* read ap version */
    ret = cs_press_iic_read(AP_VERSION_REG, read_temp, CS_FW_VERSION_LENGTH);

    if(ret >= 0)
    {
        /* get driver ap version */
        default_version = ((((unsigned short)fw_array[FW_ADDR_VERSION+0]<<8)&0xff00)|fw_array[FW_ADDR_VERSION+1]);
        /* get ic ap version */
        read_version = ((((unsigned short)read_temp[2]<<8)&0xff00)|read_temp[3]);
        /* compare */
        if(read_version < default_version)
        {
            flag_update = 1;
        }
    }
    else
    {
        flag_update = 1;

        LOG_ERR("read AP_VERSION_REG failed\n");
    }
    if(flag_update == 0)
    {
        LOG_INFO("no need update\n");
        return 1;   /* no need update */
    }
    /* update fw */
    retry = RETRY_NUM;
    do
    {
        result = cs_press_fw_force_update(fw_array);
        if(result < 0 || result >= 0x80){
            LOG_ERR("update failed,ret:%d\n",result);
        }
    }while((result!=0)&&(retry--));
    LOG_INFO("update finished,default version:%d,read version:%d\n",default_version,read_version);

    return result;
}

/**
  * @brief    fml_firmware_send_data
  * @param
  * @retval   none
  */
int fml_firmware_send_data(unsigned char *data, int len)
{
    int fw_body_len = 0;
    int fw_len = 0;
    int ret = 0;
    char result = 0;
#if defined(CONFIG_OPLUS_FEATURE_FEEDBACK) || defined(CONFIG_OPLUS_FEATURE_FEEDBACK_MODULE)
    char payload[1024] = {0x00};
#endif

    if(data == NULL){
        ret = -1;
        LOG_ERR("data buffer null:\n");
        goto exit_fw_buf;
    }
    fw_body_len = ((int)data[FW_ADDR_CODE_LENGTH]<<8)|((int)data[FW_ADDR_CODE_LENGTH+1]);
    fw_len = fw_body_len + 256;
    LOG_INFO("[fw file len:%x,fw body len: %x]\n", len, fw_body_len);
    if(fw_body_len <= 0 || fw_body_len > FW_UPDATA_MAX_LEN)
    {
        LOG_ERR("[err!fw body len err!len=%x]\n", len);
        ret = -1;
        goto exit_fw_buf;
    }
    if(fw_len != len)
    {
        LOG_ERR("[err!fw file len err! len=%x]\n", fw_len);
        ret = -1;
        goto exit_fw_buf;
    }
    set_device_updating_flag(1);
    if(g_cs_press.update_type == FORCE_FILE_UPDATE)
    {
        result = cs_press_fw_force_update(data);
        g_cs_press.update_type = HIGH_VER_FILE_UPDATE;
    }else{
        result = cs_press_fw_high_version_update(data);
    }
    set_device_updating_flag(0);
    if (result < 0 || result >= 0x80)
    {
        ret  = -1;
        LOG_ERR("Burning firmware fails\n");
#if defined(CONFIG_OPLUS_FEATURE_FEEDBACK) || defined(CONFIG_OPLUS_FEATURE_FEEDBACK_MODULE)
        memset(payload, 0, 1024);
        scnprintf(payload, sizeof(payload),
                   "NULL$$EventField@@FwUpdate$$FieldData@@FwUpdate$$detailData@@result=0x%x", result);
        LOG_INFO("%s\n", payload);
        oplus_kevent_fb(PSW_BSP_KEYPAD, CS_PRESS_FB_FW_UPDATE_TYPE, payload);
#endif
        g_cs_press.fw_update_error = result;
    }
exit_fw_buf:
    msleep(100);
    cs_press_get_noise_var(g_cs_press.dac_noise_var_boot);
    cs_press_get_offset(g_cs_press.dac_offset_boot);
#if defined(CONFIG_OPLUS_FEATURE_FEEDBACK) || defined(CONFIG_OPLUS_FEATURE_FEEDBACK_MODULE)
    memset(payload, 0, 1024);
    scnprintf(payload, sizeof(payload),
               "NULL$$EventField@@Offset$$FieldData@@Offset$$detailData@@offset[0]=%d,offset[1]=%d",
               g_cs_press.dac_offset_boot[0], g_cs_press.dac_offset_boot[1]);
    LOG_INFO("%s\n", payload);
    oplus_kevent_fb(PSW_BSP_KEYPAD, CS_PRESS_FB_OFFSET_TYPE, payload);
    memset(payload, 0, 1024);
    scnprintf(payload, sizeof(payload),
               "NULL$$EventField@@Noise$$FieldData@@NoiseVar$$detailData@@noiseVar[0]=%d,noiseVar[1]=%d",
               g_cs_press.dac_noise_var_boot[0], g_cs_press.dac_noise_var_boot[1]);
    LOG_INFO("%s\n", payload);
    oplus_kevent_fb(PSW_BSP_KEYPAD, CS_PRESS_FB_NOISE_VAR_TYPE, payload);
#endif
    LOG_INFO("end\n");
    return ret;
}

/**
  * @brief    fml_firmware_config_cb
  * @param
  * @retval   none
  */
static void fml_firmware_config_cb(const struct firmware *cfg, void *ctx)
{
    int fw_error = 0;

    mutex_lock(&press_lock);
    if(cfg)
    {
        fw_error = fml_firmware_send_data((unsigned char*)cfg->data, cfg->size);
        if(fw_error)
        {
            LOG_ERR("firmware send data err:%d\n", fw_error);
            goto err_release_cfg;
        }
    }
err_release_cfg:
    release_firmware(cfg);
    cs_press_delay_ms(10);
    //ftm_boot_mode_check();
#ifdef CAMERA_KEY
    /*camera_key_config_read(&g_cs_press.strength_cfg);*/
    camera_key_para_init();
    cs_press_set_mode(g_cs_press.camera_key_mode);
    cs_press_set_trigger_strength(g_cs_press.strength_cfg);
#endif
    mutex_unlock(&press_lock);
    LOG_INFO("end\n");
}


/**
  * @brief    firmware update by file
  * @param
  * @retval 0:success, -1: fail,1:no need update
  */
int fml_fw_update_by_file(void)
{
    int ret_error = 0;
    struct cs_press_t *fw_st = NULL;

    fw_st = devm_kzalloc(&(g_cs_press.client->dev), sizeof(*fw_st), GFP_KERNEL );
    if(!fw_st)
    {
        LOG_ERR("devm_kzalloc failed\n");
        return -1;
    }
    fw_st->client = g_cs_press.client;
    ret_error = request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG, FW_FILE_NAME, &(g_cs_press.client->dev), GFP_KERNEL,
                                        fw_st, fml_firmware_config_cb);
    devm_kfree(&(g_cs_press.client->dev), fw_st);
    if(ret_error)
    {
        LOG_ERR("request_firmware_nowait failed:%d\n",ret_error);
        return -1;
    }
    LOG_INFO("end\n");
    return 0;
}

#if 0
/**
  * @brief  fml_fw_update_by_array
  * @param
  * @retval 0:success, -1: fail
  */
int fml_fw_update_by_array(void)
{
    char ret = 0;
    char retry = RETRY_NUM;

    cs_press_wakeup_iic();
    do{
        ret = cs_press_fw_high_version_update(cs_default_fw_array);
        if(ret < 0){
            LOG_ERR("update failed,ret:%d\n",ret);
        }
    }while((ret!=0)&&(retry--));

    return ret;
}
#endif

/**
  * @brief       read fw info
  * @param[out]  *fw_info: point to info struct
  * @retval      0:success, -1: fail
  */
char cs_press_read_fw_info(CS_FW_INFO_Def *fw_info)
{
    char ret = 0;
    unsigned char read_temp[FW_ONE_BLOCK_LENGTH_R] = {0};

    if(fw_info == NULL){
        LOG_ERR("ERR:point fw_info NULL\n");
    }
    cs_press_wakeup_iic();
    /* read fw info*/
#if 0
    ret |= cs_press_iic_read(AP_MANUFACTURER_ID_REG, read_temp, CS_MANUFACTURER_ID_LENGTH);  // Manufacturer id
    if(ret >= 0)
    {
        LOG_ERR("%x,%x\n",read_temp[0],read_temp[1]);
        fw_info->manufacturer_id = ((unsigned short)read_temp[1]<<8) | read_temp[0];
        LOG_ERR("%x\n",fw_info->manufacturer_id);
    }
    ret |= cs_press_iic_read(AP_MODULE_ID_REG, read_temp, CS_MODULE_ID_LENGTH);  // Module id
    if(ret >= 0)
    {
        fw_info->module_id = ((unsigned short)read_temp[1]<<8) | read_temp[0];
    }
#endif
    ret |= cs_press_iic_read(AP_VERSION_REG, read_temp, CS_FW_VERSION_LENGTH);  /* FW Version*/
    if(ret==0)
    {
        fw_info->fw_version = ((unsigned short)read_temp[2]<<8) | read_temp[3];
    }
    return ret;
}

/**
  * @brief  wake up device
  * @param  None
  * @retval 0:success, -1: fail
  */
char cs_press_set_device_wakeup(void)
{
    char ret = 0;
    unsigned char retry = RETRY_NUM;
    unsigned char temp_data = 1;

    do
    {
        if(ret!=0)
        {
            cs_press_delay_ms(1);
        }
        ret = cs_press_iic_write(AP_WAKEUP_REG, &temp_data, 1);
    }while((ret!=0)&&(retry--));
    return ret;
}

/**
  * @brief  put the device to sleep
  * @param  None
  * @retval 0:success, -1: fail
  */
char cs_press_set_device_sleep(void)
{
    char ret = 0;
    unsigned char retry = 5;
    char i;
    /*0=FTM 0x01=write flag 0xFF=check sum*/
    unsigned char sleep_cmd[3] = {0x00, 0x01, 0xFF};
    unsigned char read_data[3] = {0x00, 0xFF, 0xFF};

    for(i = 0; i < retry; i++)
    {
        ret = cs_press_iic_write(AP_RW_WORK_MODE_REG, sleep_cmd, 3);
        if(ret == 0)
        {
            ret = -1;
            cs_press_delay_ms(1);
            if(cs_press_iic_read(AP_RW_WORK_MODE_REG, read_data, 3) == 0)
            {
                if((read_data[0]==0)&&((read_data[1]+read_data[2]) == 0))
                {
                    ret = 0;
                    break;
                }
            }
        }
        cs_press_delay_ms(1);
    }
    return ret;
}

/**
  * @brief  init read rawdata
  * @param  None
  * @retval 0:success, -1: fail
  */
char cs_press_read_rawdata_init(void)
{
    char ret = 0;

    cs_press_wakeup_iic();

    cs_press_clean_debugmode();

    ret = cs_press_set_debugmode(AP_R_RAWDATA_DEBUG_MODE);

    return ret;
}

/**
  * @brief      read rawdata
  * @param[in] *rawdata: point to sensor data strcut
  * @retval    0:none, -1: fail, >0:vaild data num
  */
char cs_press_read_rawdata(CS_RAWDATA_Def *rawdata)
{
    char ret = 0;
    unsigned char i = 0;
    unsigned char data_temp[AFE_MAX_CH*2] = {0};
    unsigned char byte_num;

    ret = (char)-1;

    byte_num = cs_press_get_debugready();
    if((byte_num <= (AFE_MAX_CH*2))&&(byte_num > 0))
    {
        if((byte_num%2) == 0)
        {
            ret = cs_press_read_debugdata(data_temp, byte_num);
            if(ret == 0)
            {
                for(i=0;i<(byte_num/2);i++)
                {
                    rawdata->rawdata[i] = ((((unsigned short)data_temp[i*2+1]<<8)&0xff00)|data_temp[i*2]);
                }
                ret = byte_num/2;
            }
        }
        else{
            ret = (char)-1;
        }
    }else{
        LOG_ERR("byte_num invalid:%d\n",byte_num);
    }
    cs_press_set_debugready(0);
    return ret;
}

/**
  * @brief  init read processed data
  * @param  None
  * @retval 0:success, -1: fail
  */
char cs_press_read_processed_data_init(void)
{
    char ret = 0;

    cs_press_wakeup_iic();

    cs_press_clean_debugmode();

    ret = cs_press_set_debugmode(AP_R_PROCESSED_DEBUG_MODE);

    return ret;
}

/**
  * @brief      read processeddata
  * @param[in]  *proce_data: point to processed data strcut
  * @retval     0:success, -1: fail
  */
char cs_press_read_processed_data(CS_PROCESSED_DATA_Def *proce_data)
{
    char ret = 0;
    unsigned char i = 0;
    unsigned char data_temp[(AFE_USE_CH*4*2)+2]; /* 1 ch have 4 types of data*/
    short data_temp_s16[(AFE_USE_CH*4)+1];
    unsigned char byte_num;
    short checksum;

    ret = (char)-1;

    byte_num = cs_press_get_debugready();

    if((byte_num == ((AFE_USE_CH*4*2)+2))&&(byte_num > 0))
    {
        ret = cs_press_read_debugdata(data_temp, byte_num);
        if(ret == 0)
        {
            for(i=0;i<(AFE_USE_CH*4)+1;i++)
            {
                data_temp_s16[i] = (short)((((unsigned short)data_temp[i*2+1]<<8)&0xff00)|data_temp[i*2]);
            }
            checksum = 0;
            for(i=0;i<(AFE_USE_CH*4);i++)
            {
                checksum += data_temp_s16[i];
            }
            if(checksum == data_temp_s16[(AFE_USE_CH*4)])    /* check right*/
            {
                for(i=0;i<AFE_USE_CH;i++)
                {
                    proce_data->arith_rawdata[i] = data_temp_s16[i];
                    proce_data->baseline[i] = data_temp_s16[AFE_USE_CH+i];
                    proce_data->diffdata[i] = data_temp_s16[AFE_USE_CH*2+i];
                    proce_data->energy_data[i] = data_temp_s16[AFE_USE_CH*3+i];
                }
                ret = 0;
            }
        }else{
            LOG_ERR("byte_num invalid:%d\n",byte_num);
        }
    }
    cs_press_set_debugready(0);
    return ret;
}

/**
  * @brief  init read processed data
  * @param  None
  * @retval 0:success, -1: fail
  */
char cs_press_read_ndt_debug_init(void)
{
    char ret = 0;
    unsigned char retry = RETRY_NUM;
    cs_press_wakeup_iic();
    cs_press_clean_debugmode();
    do
    {
        ret = cs_press_set_debugmode(AP_R_NDT_DEBUG_MODE);
    }while((ret != 0 ) && (retry--));

    LOG_ERR("debug init %d\n",ret);
    return ret;
}

/**
  * @brief      read debug data
  * @param[in]
  * @retval     0:success, -1: fail
  */
char cs_press_read_ndt_debug_data(short int *debug_data, unsigned char *len)
{
    char ret = 0;
    unsigned char i = 0;
    unsigned char data_temp[256];
    unsigned char byte_num = 0;
    short int checksum = 0;

    byte_num = cs_press_get_debugready();
    if(byte_num > 1)
    {
        *len = byte_num/2;
        ret = cs_press_read_debugdata(data_temp, byte_num);
        if(ret == 0)
        {
            for(i = 0; i < byte_num/2; i++)
            {
                debug_data[i] = (short int)((((unsigned short)data_temp[i*2+1]<<8)&0xff00)|data_temp[i*2]);
            }
            /*check sum*/
            for(i = 0; i < byte_num/2 ; i++)
            {
                checksum += debug_data[i];
            }
            if(checksum != 0)
            {
                ret = -1;
                LOG_ERR("checksum err %d\n",checksum);
            }
        }
        cs_press_set_debugready(0);
    }else{
        ret = -1;
    }
    return ret;
}
/**
  * @brief        read calibration factor
  * @param[in]    read channel num
  * @param[out]   *cal_factor: point to calibration factor
  * @retval       0:success, -1: fail
  */
char cs_press_read_calibration_factor(unsigned char ch, unsigned short *cal_factor)
{
    char ret = (char)-1;
    unsigned char data_temp[4]={0,0,0,0};
    unsigned char num;

    if(cal_factor == NULL){
        LOG_ERR("point cal_factor if NULL\n");
        return -1;
    }
    cs_press_wakeup_iic();
    cs_press_clean_debugmode();
    cs_press_set_debugmode(AP_R_CAL_FACTOR_DEBUG_MODE);
    data_temp[0] = ch;
    cs_press_write_debugdata(data_temp, 2);
    cs_press_set_debugready(2);
    cs_press_delay_ms(DEBUG_MODE_DELAY_TIME);
    num = cs_press_get_debugready();
    if(num == 4)
    {
        ret = cs_press_read_debugdata(data_temp, 4);
        if(ret == 0)
        {
            *cal_factor = ((((unsigned short)data_temp[1]<<8)&0xff00)|data_temp[0]);
        }
    }
    return ret;
}

/**
  * @brief      write calibration factor
  * @param[in]  channel num
  * @param[in]  cal_factor: calibration factor data
  * @retval     0:success, -1: fail
  */
char cs_press_write_calibration_factor(unsigned char ch, unsigned short cal_factor)
{
    char ret = 0;
    unsigned char data_temp[7] = {0};
    unsigned short read_cal_factor = 0;

    cs_press_wakeup_iic();
    cs_press_clean_debugmode();
    cs_press_set_debugmode(AP_W_CAL_FACTOR_DEBUG_MODE);

    data_temp[0] = ch;
    data_temp[1] = 0;
    data_temp[2] = cal_factor;
    data_temp[3] = cal_factor>>8;
    data_temp[4] = 0;
    data_temp[5] = 0;

    cs_press_write_debugdata(data_temp, 6);
    cs_press_set_debugready(6);
    cs_press_delay_ms(DEBUG_MODE_DELAY_TIME*3);
    cs_press_read_calibration_factor(ch, &read_cal_factor);
    if(read_cal_factor != cal_factor)
    {
        LOG_ERR("set calibration factor failed\n");
        ret = (char)-1;
    }
    return ret;
}
/**
  * @brief      enable calibration function
  * @param[in]  channel num
  * @retval 0:success, -1: fail
  */
char cs_press_calibration_enable(unsigned char ch_num)
{
    char ret = 0;
    unsigned char temp_data = 1 + ch_num;

    cs_press_wakeup_iic();
    cs_press_delay_ms(1);
    ret = cs_press_iic_write(AP_CALIBRATION_REG, &temp_data, 1);
    return ret;
}

/**
  * @brief  disable calibration function
  * @param  None
  * @retval 0:success, -1: fail
  */
char cs_press_calibration_disable(void)
{
    char ret = 0;
    unsigned char temp_data = 0;

    cs_press_wakeup_iic();
    cs_press_delay_ms(1);
    ret = cs_press_iic_write(AP_CALIBRATION_REG, &temp_data, 1);
    return ret;

}
/**
  * @brief       read press level
  * @param[out]  *press_level: point to press level buffer
  * @retval      0:success, -1: fail
  */
char cs_press_read_press_level(unsigned short *press_level)
{
    char ret = (char)-1;
    unsigned char data_temp[4]={0,0,0,0};
    unsigned char num;

    if(press_level == NULL){
        LOG_ERR("point press_level is NULL\n");
        return -1;
    }
    cs_press_wakeup_iic();
    cs_press_clean_debugmode();
    cs_press_set_debugmode(AP_R_PRESS_LEVEL_DEBUG_MODE);
    cs_press_write_debugdata(data_temp, 2);
    cs_press_set_debugready(2);
    cs_press_delay_ms(DEBUG_MODE_DELAY_TIME);
    num = cs_press_get_debugready();
    if(num == 4)
    {
        ret = cs_press_read_debugdata(data_temp, num);
        if(ret == 0)
        {
            *press_level = ((((unsigned short)data_temp[1]<<8)&0xff00)|data_temp[0]);
        }
    }
    return ret;
}

/**
  * @brief      write press level
  * @param[in]  press_level: press level data
  * @retval     :success, -1: fail
  */
char cs_press_write_press_level(unsigned short press_level)
{
    char ret;
    unsigned char data_temp[6]={0};
    unsigned short read_press_level = 0;

    cs_press_wakeup_iic();

    cs_press_clean_debugmode();
    cs_press_set_debugmode(AP_W_PRESS_LEVEL_DEBUG_MODE);

    data_temp[0] = 0;
    data_temp[1] = 0;
    data_temp[2] = press_level;
    data_temp[3] = press_level>>8;
    data_temp[4] = 0;
    data_temp[5] = 0;

    cs_press_write_debugdata(data_temp, 6);
    cs_press_set_debugready(6);
    cs_press_delay_ms(DEBUG_MODE_DELAY_TIME*3);
    cs_press_read_press_level(&read_press_level);

    ret = 0;
    if(read_press_level != press_level)
    {
        ret = (char)-1;
    }

    return ret;
}
/**
  * @brief      check calibration result
  * @param[out] check result data buf
  * @retval     0:running, -1: error, 1: success, 2: fail, 3: overtime
  */
char cs_press_calibration_check(CS_CALIBRATION_RESULT_Def *calibration_result)
{
    char ret;
    unsigned char num;
    unsigned char data_temp[10];

    if(calibration_result == NULL){
        LOG_ERR("point calibration_result is NULL\n");
        return -1;
    }
    ret = cs_press_set_debugmode(AP_CALIBRATION_DEBUG_MODE);
    num = cs_press_get_debugready();
    if(num == 10)
    {
        ret = cs_press_read_debugdata(data_temp, 10);

        if(ret == 0)
        {
            calibration_result->calibration_channel = data_temp[0];
            calibration_result->calibration_progress = data_temp[1];
            calibration_result->calibration_factor = ((((unsigned short)data_temp[3]<<8)&0xff00)|data_temp[2]);
            calibration_result->press_adc_1st = ((((short)data_temp[5]<<8)&0xff00)|data_temp[4]);
            calibration_result->press_adc_2nd = ((((short)data_temp[7]<<8)&0xff00)|data_temp[6]);
            calibration_result->press_adc_3rd = ((((short)data_temp[9]<<8)&0xff00)|data_temp[8]);

            if(calibration_result->calibration_progress >= CALIBRATION_SUCCESS_FALG)
            {
                ret = 1;
            }
            if(calibration_result->calibration_progress == CALIBRATION_FAIL_FALG)
            {
                ret = 2;
            }
            if(calibration_result->calibration_progress == CALIBRATION_OVERTIME_FALG)
            {
                ret = 3;
            }
        }
    }

    return ret;
}

/**
  * @brief      init read noise data
  * @param[in]  period_num: period num of calculation the noise
  * @retval     0:success, -1: fail
  */
char cs_press_read_noise_init(unsigned short period_num)
{
    char ret = 0;
    unsigned char data_temp[2];

    cs_press_wakeup_iic();

    ret |= cs_press_clean_debugmode();

    ret |= cs_press_set_debugmode(AP_R_NOISE_DEBUG_MODE);

    data_temp[0] = period_num;
    data_temp[1] = period_num>>8;
    ret |= cs_press_write_debugdata(data_temp, 2);

    ret |= cs_press_set_debugready(2);

    return ret;
}

/**
  * @brief      read noise data
  * @param[out] noise_data:read data buf
  * @retval     0:success, -1: fail
  */
char cs_press_read_noise(CS_NOISE_DATA_Def *noise_data)
{
    char ret = 0;
    unsigned char i = 0;
    unsigned char num = 0;
    unsigned char data_temp[AFE_USE_CH*6];
    unsigned int dat_temp;

    if(noise_data == NULL){
        LOG_ERR("point noise_data is NULL\n");
        return -1;
    }
    ret = (char)-1;

    cs_press_wakeup_iic();

    num = cs_press_get_debugready();

    if(num == AFE_USE_CH*6)
    {
        cs_press_read_debugdata(data_temp, num);

        dat_temp = 0;
        for(i=0;i<AFE_USE_CH;i++)
        {
            noise_data->noise_peak[i] = ((((short)data_temp[1+i*6]<<8)&0xff00)|data_temp[0+i*6]);

            dat_temp = data_temp[5+i*6];
            dat_temp <<= 8;
            dat_temp |= data_temp[4+i*6];
            dat_temp <<= 8;
            dat_temp |= data_temp[3+i*6];
            dat_temp <<= 8;
            dat_temp |= data_temp[2+i*6];

            noise_data->noise_std[i] = dat_temp;
        }
        ret = 0;
    }
    return ret;
}
/**
  * @brief  init read offset data
  * @param  None
  * @retval 0:success, -1: fail
  */
static char cs_press_read_offset_init(void)
{
    char ret = 0;

    cs_press_wakeup_iic();
    ret |= cs_press_clean_debugmode();
    ret |= cs_press_set_debugmode(AP_R_OFFSET_DEBUG_MODE);
    ret |= cs_press_set_debugready(0);
    return ret;
}

/**
  * @brief          read offset data
  * @param[out]      offset_data
  * @retval         0:success, -1: fail
  */
char cs_press_read_offset(CS_OFFSET_DATA_Def *offset_data)
{
    char ret;
    unsigned char i;
    unsigned char num;
    unsigned char data_temp[AFE_USE_CH*2]={0};

    if(offset_data == NULL){
        LOG_ERR("point offset_data is NULL\n");
        return -1;
    }
    ret = (char)-1;
    cs_press_read_offset_init();

    cs_press_delay_ms(50);                    /* waitting for data ready*/

    num = cs_press_get_debugready();

    if(num == (AFE_USE_CH*2))
    {
        cs_press_read_debugdata(data_temp, num);
        for(i = 0; i < AFE_USE_CH; i++)
        {
            offset_data->offset[i] = ((((short)data_temp[i*2+1]<<8)&0xff00)|(short)data_temp[i*2]);
        }
        ret = 0;
    }
    return ret;
}
/**
  * @brief  init read sensor status
  * @param  None
  * @retval 0:success, -1: fail
  */
static char cs_press_read_sensor_status_init(void)
{
    char ret = 0;

    cs_press_wakeup_iic();
    ret |= cs_press_clean_debugmode();
    ret |= cs_press_set_debugready(0);
    ret |= cs_press_set_debugmode(AP_R_SENSOR_STATUS_DEBUG_MODE);

    return ret;
}

/**
  * @brief      read noise data
  * @param[out] sensor_status
  * @retval     0:success, -1: fail
  */
char cs_press_read_sensor_status(CS_SENSOR_STATUS_Def *sensor_status)
{
    char ret;
    unsigned char i;
    unsigned char num;
    unsigned char data_temp[AFE_USE_CH]={0};

    if(sensor_status == NULL){
        LOG_ERR("point sensor_status is NULL\n");
        return -1;
    }
    ret = (char)-1;

    cs_press_read_sensor_status_init();

    for(i=0;i<AFE_USE_CH;i++)
    {
        cs_press_delay_ms(50);                  /* waitting for data ready */
    }

    num = cs_press_get_debugready();

    if(num == (AFE_USE_CH))
    {
        cs_press_read_debugdata(data_temp, num);

        for(i=0;i<AFE_USE_CH;i++)
        {
            sensor_status->status[i] = (short)data_temp[i];
        }
        ret = 0;
    }

    return ret;
}

char cs_write_calibration_command(unsigned char *data)
{
    char ret = 0;

    if(data == NULL){
        LOG_ERR("point data is NULL\n");
        return -1;
    }
    ret |= cs_press_wakeup_iic();
    ret |= cs_press_clean_debugmode();
    ret |= cs_press_set_debugmode(0x14);
    ret |= cs_press_read_debugdata(data, 2);
    return ret;
}

char cs_boot_to_ap_cmd(void)
{
    char ret = 0 ;
    unsigned char boot_fw_jump_cmd[BOOT_CMD_LENGTH] = BOOT_FW_JUMP_CMD;
    ret = cs_press_iic_write_double_reg(0xF17C ,boot_fw_jump_cmd, BOOT_CMD_LENGTH);

    return ret ;
}

int cs_read_boot_version(unsigned char *version_data)
{
    int ret = 0 ;

    if(version_data == NULL){
        LOG_ERR("point version_data is NULL\n");
        return -1;
    }
    cs_press_reset_ic();
    ret = cs_press_iic_read_double_reg(0xF100, version_data, 4);
    return ret;
}

unsigned char check_sum(unsigned char *pbuf, unsigned char len)
{
    unsigned char cs = 0;
    unsigned char i = 0;
    for(i = 0; i < len; i++){
        cs += pbuf[i];
    }
    return cs;
}


char cs_set_press_threshold(int left_val, int right_val)
{
    int i;
    int ret = 0 ;
    unsigned char retry = RETRY_NUM;
    unsigned char temp_data[CS_FORCE_TRIG_LENGTH];

    temp_data[0] = left_val&0xff;
    temp_data[1] = (left_val>>8)&0xff;
    temp_data[2] = right_val&0xff;
    temp_data[3] = (right_val>>8)&0xff;
    temp_data[4] = 0x03;/*set left & right*/
    temp_data[5] = 0;
    for(i = 0; i < 5; i++)
    {
        temp_data[5] += temp_data[i];
    }
    temp_data[5] = (0xFF - temp_data[5]) + 1;
    do
    {
        if(ret!=0)
        {
            cs_press_delay_ms(1);
        }
        ret = cs_press_iic_write(AP_RW_BUTTON_FORCE_THR_REG, temp_data, CS_FORCE_TRIG_LENGTH);
    }while((ret != 0 ) && (retry--));
    if(ret < 0)
    {
        LOG_ERR("err write press trig\n");
    }else{
        LOG_DEBUG("write press trig:ok %d %d\n",left_val, right_val);
    }
    return ret;
}

/**************************
 * reg = 0x52 len=4 
 * byte0 0x01 up screen, 0x02 down screen, 0x00 no touch
 * byte1 0x0A 0x0B 0x0C 0x0D, 0x00 no area
 * byte2 
 *screen_half :0x01 up screen, 0x02 down screen, 0x00 no touch
 *screen_area :0x0A 0x0B 0x0C 0x0D, 0x00 no area
 *************************/
char cs_write_tp_info (unsigned char screen_half, unsigned char screen_area)
{
    char ret = 0 ;
    unsigned char retry = RETRY_NUM;
    unsigned char temp_data[6];
    static unsigned char pre_screen_half = 0xFF, pre_screen_area = 0xFF;
    /*****not in game mode return****/
    if(g_cs_press.cs_shell_themal_enable == 0){
        LOG_ERR("not in game mode\n");
        return -1;
    }
     /*****fw updating return****/
    if(get_device_updating_flag()){
        LOG_ERR("updating\n");
        return -2;
    }

    if((pre_screen_half == screen_half)&&(pre_screen_area == screen_area)){
        LOG_ERR("same with last time\n");
        return -3;
    }

    temp_data[0] = screen_half;
    temp_data[1] = screen_area;
    temp_data[2] = 0x01;
    temp_data[3] = 0-check_sum(temp_data, 3);
    do
    {
        if(ret != 0)
        {
            cs_press_delay_ms(1);
        }
        ret = cs_press_iic_write(AP_RD_TP_INFO_REG, temp_data, 4);
    }while((ret != 0 ) && (retry--));
    if(ret == 0 ){
        pre_screen_half = screen_half;
        pre_screen_area = screen_area;
    }

    return ret;
}
EXPORT_SYMBOL_GPL(cs_write_tp_info);

/********************
*0 leather, 1 glass
*************************/
char cs_write_shell_type (unsigned char shell_type)
{
    char ret = 0 ;
    unsigned char retry = RETRY_NUM;
    unsigned char temp_data[6];

     /*****fw updating return****/
    if(get_device_updating_flag()){
        LOG_ERR("updating\n");
        return -2;
    }


    temp_data[0] = shell_type;
    temp_data[1] = 0x01;
    temp_data[2] = 0 - check_sum(temp_data, 2);
    do
    {
        if(ret != 0)
        {
            cs_press_delay_ms(1);
        }
        ret = cs_press_iic_write(AP_RD_TP_INFO_REG, temp_data, 4);
    }while((ret != 0 ) && (retry--));
    if(ret >= 0){
        LOG_DEBUG("shell type ok %d\n", shell_type);
    }else{
        LOG_DEBUG("shell type err %d\n", shell_type);
    }

    return ret;
}

/********************
*0 no charge, 1 charging
*************************/
void cs_write_charge_state (unsigned char state)
{
    if(state <= 1){
        g_cs_press.charge_flag = state;
        /*game mode*/
        if(g_cs_press.cs_shell_themal_enable == 1){
            cancel_delayed_work(&g_cs_press.delay_run_worker);
            schedule_delayed_work(&g_cs_press.delay_run_worker, msecs_to_jiffies(DEFAULT_RUN_DELAY_TIME));
        }
    }
}

unsigned char cs_read_charge_state (void)
{
    return g_cs_press.charge_flag;
}
/*
static void ftm_boot_mode_check(void)
{
    int boot_mode = 0;

#ifndef ALIENTEK
    boot_mode = get_boot_mode();
    LOG_DEBUG("ftm boot_mode %d\n", boot_mode);

    if ((boot_mode == MSM_BOOT_MODE__FACTORY
         || boot_mode == MSM_BOOT_MODE__RF || boot_mode == MSM_BOOT_MODE__WLAN))
#else
    if(boot_mode != 0)
#endif
    {
        /set f71 into sleep mode/
        if(cs_press_set_device_sleep() == 0)
        {
            LOG_DEBUG("ftm into sleep mode ok\n");
        }else{
            LOG_DEBUG("ftm into sleep mode fail\n");
        }
    }else{
        LOG_DEBUG("do not at ftm\n");
        cancel_delayed_work(&g_cs_press.delay_run_worker);
        schedule_delayed_work(&g_cs_press.delay_run_worker, msecs_to_jiffies(DEFAULT_RUN_DELAY_TIME));
#if 0
    #if NORMAL_GEAT_2LEVLE_EN
        g_cs_press.normal_left_geat_val = normal_left_geat[0];
        g_cs_press.normal_right_geat_val = normal_right_geat[0];
    #else
        g_cs_press.normal_left_geat_val = normal_left_geat;
        g_cs_press.normal_right_geat_val = normal_right_geat;
    #endif
        //cs_set_press_threshold(g_cs_press.normal_left_geat_val, g_cs_press.normal_right_geat_val);
        temp_data[0] = 0x01;
        temp_data[1] = 0x01;
        temp_data[2] = 0xFE;
        retry = RETRY_NUM;
        do
        {
            if(ret!=0)
            {
                cs_press_delay_ms(1);
            }
            ret = cs_press_iic_write(AP_RW_WORK_MODE_REG, temp_data, 3);
        }while((ret != 0 ) && (retry--));
        if(ret < 0)
        {
            LOG_ERR("enter sleep 1Hz faill\n");
        }else{
            LOG_DEBUG("enter sleep 1Hz ok\n");
        }
#endif
    }
}
*/

/**
  * @brief  init config para
  * @param  None
  * @retval 0:success, -1: fail
  */
char cs_press_para_init(void)
{
    int rc;
    int i;
    char ret = 0;
    struct device_node *np;
    struct device_node *ch_node = NULL;
    int geat_num = 0;

    struct scene_para_t *p_new = NULL;
    struct scene_para_t temp_scene_para;
    struct scene_para_t *ptemp_list = NULL;

    const char *pscene_name = NULL;

    np = g_cs_press.client->dev.of_node;
#if 0
    int temp_array[3];   
#if NORMAL_GEAT_2LEVLE_EN
    rc = of_property_read_u32_array(np, "press,normal-left-geat", temp_array, 2);
    if (rc < 0) {
        //LOG_DEBUG("error:can not load normal_geat\n");
        ret |= 0x01;
    } else {
        normal_left_geat[0] = temp_array[0];
        normal_left_geat[1] = temp_array[1];
        //LOG_DEBUG("left_geat %d %d\n", normal_left_geat[0], normal_left_geat[1]);
    }
    rc = of_property_read_u32_array(np, "press,normal-right-geat", temp_array, 2);
    if (rc < 0) {
        //LOG_DEBUG("error:can not load normal_geat\n");
        ret |= 0x02;
    } else {
        normal_right_geat[0] = temp_array[0];
        normal_right_geat[1] = temp_array[1];
        LOG_DEBUG("right_geat %d %d\n", normal_right_geat[0], normal_right_geat[1]);
    }
#else
    int temp_geat;
    rc = of_property_read_u32(np, "press,normal-left-geat", &temp_geat);
    if (rc < 0) {
        LOG_DEBUG("error:can not load normal_left_geat\n");
        ret |= 0x01;
    } else {
        normal_left_geat = temp_geat;
        LOG_DEBUG("normal_left_geat %d\n", temp_geat);
    }

    rc = of_property_read_u32(np, "press,normal-right-geat", &temp_geat);
    if (rc < 0) {
        LOG_DEBUG("error:can not load normal_right_geat\n");
        ret |= 0x02;
    } else {
        normal_right_geat = temp_geat;
        LOG_DEBUG("normal_right_geat %d\n", temp_geat);
    }
#endif
    rc = of_property_read_u32_array(np, "press,game-geat", temp_array, 3);
    if (rc < 0) {
        //LOG_DEBUG("error:can not load game_geat\n");
        ret |= 0x04;
    } else {
        game_geat[0] = temp_array[0];
        game_geat[1] = temp_array[1];
        game_geat[2] = temp_array[2];
        LOG_DEBUG("game_geat %d %d %d\n", game_geat[0], game_geat[1], game_geat[2]);
    }
#endif

    LOG_DEBUG("for_each_child_of_node:\n");
    for_each_child_of_node(np, ch_node) {
        rc = of_property_read_string(ch_node, "scene,name", &pscene_name);
        strncpy(temp_scene_para.scene_name, pscene_name, MIN(strlen(pscene_name)+1, NAME_MAX_LENS));

        geat_num = of_property_count_u32_elems(ch_node, "press,left-geat");
        if(geat_num > PRESS_LEVEL_NUM){
            geat_num = PRESS_LEVEL_NUM;
        }
        rc |= of_property_read_u32_array(ch_node, "press,left-geat",
                                        temp_scene_para.left_geat, geat_num);

        geat_num = of_property_count_u32_elems(ch_node, "press,right-geat");
        if(geat_num > PRESS_LEVEL_NUM){
            geat_num = PRESS_LEVEL_NUM;
        }
        rc |= of_property_read_u32_array(ch_node, "press,right-geat",
                                        temp_scene_para.right_geat, geat_num);

        geat_num = of_property_count_u32_elems(ch_node, "press,charge-right-geat");
        if(geat_num > PRESS_LEVEL_NUM){
            geat_num = PRESS_LEVEL_NUM;
        }
        if(geat_num > 0){
            of_property_read_u32_array(ch_node, "press,charge-right-geat",
                                        temp_scene_para.charge_right_geat, geat_num);
            for(i = 0; i < PRESS_LEVEL_NUM; i++){
                LOG_INFO("charge_right_geat[%d]=%d\n", i, temp_scene_para.charge_right_geat[i]);
            }
        }else{
            for(i = 0; i < PRESS_LEVEL_NUM; i++){
                temp_scene_para.charge_right_geat[i] = 200;
            }
        }

        rc |= of_property_read_u32(ch_node, "press,samplemode", &temp_scene_para.sample_mode);

        rc |= of_property_read_u32(ch_node, "press,priority", &temp_scene_para.priority);

        if (rc >= 0){
            p_new = (struct scene_para_t *)kzalloc(sizeof(struct scene_para_t), GFP_KERNEL);
            scene_para_copy(p_new,&temp_scene_para);
            list_add(&p_new->node, &gScenes);
        }
    }

    list_for_each_entry(ptemp_list, &gScenes, node)
    {
        if(ptemp_list)
        {
            LOG_INFO("name =\"%s\"\n",ptemp_list->scene_name);
            LOG_INFO("l_geat\tr_geat\n");
            for (i = 0; i < PRESS_LEVEL_NUM; i++) {
                LOG_INFO("%d\t\t%d\n", ptemp_list->left_geat[i], ptemp_list->right_geat[i]);
            }
            LOG_INFO("sample = %d\n",ptemp_list->sample_mode);
            LOG_INFO("priority = %d\n",ptemp_list->priority);
        }
    }
    LOG_DEBUG("for_each_child_of_node end\n");
    return ret;
}

/**
  * @brief  init
  * @param  None
  * @retval 0:success, -1: fail
  */
int cs_press_init(void)
{
    int ret = 0;
    char i;
    unsigned char boot_ver_buf[4];
    LOG_DEBUG("cs driver ver %s\n", cs_driver_ver);
    for(i = 0; i < 3; i++){
        if(cs_read_boot_version(boot_ver_buf) >= 0){
            LOG_INFO("boot version %02x %02x %02x %02x\n", boot_ver_buf[0], boot_ver_buf[1],
                                                           boot_ver_buf[2], boot_ver_buf[3]);
            if(boot_ver_buf[0] == 0x71)
            {
                break;
            }
        }else{
            LOG_ERR("read boot version err %d\n",i);
        }
    }
    if(i >= 3)
    {
        LOG_ERR("chipid err return\n");
        return -1;
    }
    /* reset ic */
    /* check whether need to update ap fw */
    ret = fml_fw_update_by_file();
    /*ret = fml_fw_update_by_array();*/

    if(ret >= 0)
    {
        return 0;
    }
    return ret;
}

#if 0
static ssize_t fml_fw_update_by_array_show(struct device *dev,
                struct device_attribute *attr, char *buf)
{
    ssize_t ret = 0;
    int result = 0;

    result = fml_fw_update_by_array();
    if (result >= 0)
        ret = sprintf(buf, "%d,pass!\n", result);
    else
        ret = sprintf(buf, "%d,failed!\n", result);

    LOG_ERR("%s\n", buf);
    return ret+1;
}
#endif

static int cs_proc_rw_reg_show(struct seq_file *m,void *v)
{
    int i = 0;

    if ((g_cs_press.read_reg_len != 0)&&(g_cs_press.read_reg_len < 255)) {
        for (i = 0; i < g_cs_press.read_reg_len; i++)
        {
            seq_printf(m, "%02x ", g_cs_press.read_reg_data[i]);
        }
        seq_printf(m, "\n");
    }else{
        seq_printf(m,"rw reg failed\n");
    }
    return 0;
}

static int cs_proc_rw_reg_open(struct inode *inode, struct file *filp)
{
    LOG_INFO("reg open start\n");
    return single_open(filp,cs_proc_rw_reg_show,NULL);
}

static ssize_t cs_proc_rw_reg_write(struct file *file, const char __user *buf,
        size_t count, loff_t *offset)
{
    char *kbuf = NULL;
    const char *startpos = NULL;
    unsigned int tempdata = 0;
    const char *lastc = NULL;
    char *firstc = NULL;
    int idx = 0;
    int ret = 0;
    int set_mode = 0;
    unsigned char change_val[32] = {0};

    LOG_INFO("reg write start\n");
    if(count <= 0){
        LOG_ERR("PARAM invalid\n");
        goto exit;
    }
    kbuf = kzalloc(count, GFP_KERNEL );
    if (!kbuf) {
        goto exit;
    }
    if (copy_from_user(kbuf, buf, count)) {
        goto exit_kfree;
    }
    startpos = kbuf;
    lastc = kbuf + count;
    while (startpos < lastc)
    {
        LOG_DEBUG("idx:%d\n", idx);
        firstc = strstr(startpos, "0x");
        if (!firstc) {
            LOG_ERR("cant find firstc\n");
            goto exit_kfree;
        }
        firstc[4] = 0;
        ret = kstrtouint(startpos, 0, &tempdata);
        if (ret) {
            LOG_ERR("fail to covert digit\n");
            goto exit_kfree;
        }
        if (idx == 0) {
            set_mode = tempdata;
            LOG_ERR("set_mode:%d\n", set_mode);
        } else {
            change_val[idx - 1] = tempdata;
            LOG_DEBUG("tempdata:%d\n", tempdata);
        }
        startpos = firstc + 5;
        idx++;
        if (set_mode == 0 && idx > 3 && idx >= change_val[0] + 3)
            break;
        else if (set_mode == 1 && idx > 3)
            break;
        else if (set_mode == 2 && idx > 4 && idx >= change_val[0] + 4)
            break;
        else if (set_mode == 3 && idx > 4)
            break;
    }

    cs_press_set_device_wakeup();
    if (set_mode == 0) {
        cs_press_iic_write(change_val[1],
            &change_val[2], (unsigned int)change_val[0]);
        g_cs_press.read_reg_len = 0;
    } else if (set_mode == 1) {
        cs_press_iic_read(change_val[1],
            &g_cs_press.read_reg_data[0], (unsigned int)change_val[0]);
        g_cs_press.read_reg_len = change_val[0];
    } else if (set_mode == 2) {
        cs_press_iic_write_double_reg(((change_val[1]<<8) | change_val[2]),
            &change_val[3], (unsigned int)change_val[0]);
        g_cs_press.read_reg_len = 0;
    } else if (set_mode == 3) {
        cs_press_iic_read_double_reg(((change_val[1]<<8) | change_val[2]),
            &g_cs_press.read_reg_data[0], (unsigned int)change_val[0]);
        g_cs_press.read_reg_len = change_val[0];
    } else {
        g_cs_press.read_reg_len = 0;
    }
exit_kfree:
    kfree(kbuf);
exit:
    return count;
}

static ssize_t cs_proc_iic_rw_test_write(struct file *file, const char __user *buf,
    size_t count, loff_t *offset)
{
    char *kbuf = NULL;
    int ret = 0;
    const char *startpos = NULL;
    char *firstc = NULL;
    unsigned int tempdata = 0;

    if(count <= 0){
        LOG_ERR("argument err\n");
        goto exit;
    }
    kbuf = kzalloc(count, GFP_KERNEL );
    if (!kbuf) {
        goto exit;
    }
    startpos = kbuf;
    if (copy_from_user(kbuf, buf, count)) {
        goto exit_kfree;
    }
    firstc = strstr(startpos, "0x");
    if (!firstc) {
        LOG_ERR("param format invalid\n");
        goto exit_kfree;
    }
    firstc[4] = 0;
    ret = kstrtouint(startpos, 0, &tempdata);
    if (ret) {
        LOG_ERR("param convert to int failed\n");
        goto exit_kfree;
    }
    ret = cs_press_iic_rw_test(tempdata);
    if(ret < 0){
        LOG_ERR("iic_rw_test:err:%d\n",ret);
    }else{
        LOG_ERR("iic_rw_test:ok:%d\n",ret);
    }
exit_kfree:
    kfree(kbuf);
exit:
    return count;
}

/**
  * @brief      set_debug_mode
  * @param[in]  reg addr, debug mode
  * @retval     0: success, -1: fail
  */
int set_debug_mode(unsigned char addr, unsigned char data)
{
    int ret = 0;
    int len = 2;
    unsigned char reg_addr;
    unsigned char reg_data[2];

    reg_addr = addr;
    len = 1;
    reg_data[0] = data;
    ret |= cs_press_iic_write(reg_addr, reg_data, len);
    if (ret == 0)
    {
        LOG_DEBUG("reg = 0x%x, data = 0x%x, len = %d\n",
            reg_addr, reg_data[0], len);
    }

    reg_addr = DEBUG_READY_REG;
    len = 1;
    reg_data[0] = 0; /* clear ready num */
    ret |= cs_press_iic_write(reg_addr, reg_data, len);
    if (ret == 0) {
        LOG_DEBUG("reg = 0x%x, data = 0x%x, len = %d\n",
            reg_addr, reg_data[0], len);
    }
    return ret;
}

/**
  * @brief      get_debug_data_ready
  * @param[in]  reg addr
  * @retval     ready data num
  */
int get_debug_data_ready(unsigned char addr)
{
    int ret = 0;
    int len = 1;
    unsigned char reg_addr;
    unsigned char reg_data[1];

    reg_addr = addr;
    len = 1;
    reg_data[0] = 0;
    ret |= cs_press_iic_read(reg_addr, reg_data, len);
    if (ret == 0) {
        LOG_DEBUG("reg = 0x%x, data = 0x%x, len = %d\n", reg_addr, reg_data[0], len);
    }else{
        LOG_ERR("reg = 0x%x, data = 0x%x, len = %d, err\n", reg_addr, reg_data[0], len);
    }
    return (int)reg_data[0];
}

/**
  * @brief      get_debug_data
  * @param[in]  reg  len
  * @param[out] ready data
  * @retval     0: success, -1: fail
  */
int get_debug_data(unsigned char addr, unsigned char* data, int len)
{
    int ret = 0;
    unsigned char reg_addr;
    unsigned char *reg_data;
    int i;

    reg_data = (unsigned char *)kmalloc(len, GFP_KERNEL );
    if (NULL == reg_data) {
        LOG_ERR("kmalloc fails. line : %d\n", __LINE__);
        return -1;
    }
    memset(reg_data, 0, len);
    reg_addr = addr;
    reg_data[0] = 0;
    ret = cs_press_iic_read(reg_addr, reg_data, len);
    if (ret == 0) {
        LOG_DEBUG("reg = 0x%x, data = 0x%x, len = %d, err\n",
            reg_addr, reg_data[0], len);
    }
    for(i = 0 ; i < len ; i++)
        data[i] = reg_data[i];

    if (NULL != reg_data) {
        kfree(reg_data);
    }
    return ret;
}

/**
  * @brief      check calibration result
  * @param[out] check result data buf
  * @retval     0:running, -1: error, 1: success, 2: fail, 3: overtime
  */
static int cs_proc_get_rawdata_show(struct seq_file *m,void *v)
{
    int ret = 0;
    int reg_len = 64;
    unsigned char reg_addr;
    unsigned char reg_data[64];
    int each_raw_size = 2;
    int len = 16;
    short raw_data[16];
    int i;
    int retry = 50;

    cs_press_wakeup_iic();
    set_debug_mode(DEBUG_MODE_REG, DEBUG_RAW_MODE);
    do {
        msleep(5);
        len = get_debug_data_ready((unsigned char)DEBUG_READY_REG);
        if (len > 0) {
            ret = get_debug_data(DEBUG_DATA_REG, reg_data, len);
            seq_printf(m,"D0 : %d, D1 : %d, D2 : %d, len : %d\n", reg_data[0], reg_data[1], reg_data[2], len);
        }
        retry--;
    } while ((len == 0) && retry > 0);

    if (len > reg_len)
    {
        len = reg_len;
    }
    if (len > 0) {
        for (i = 0 ; i < CH_NUM ; i++) {
            raw_data[i] = ((short)(reg_data[i * each_raw_size] & 0xff)
                | (short)((reg_data[i * each_raw_size + 1] << 8) & 0xff00));
            seq_printf(m,"raw_data %d = %d\n", i, raw_data[i]);
        }
        seq_printf(m,"\n");
    } else {
        seq_printf(m,"Data is not ready yet!\n");
    }
    set_debug_mode(DEBUG_MODE_REG, DEBUG_CLEAR_MODE);
    reg_addr = DEBUG_READY_REG;
    len = 1;
    reg_data[0] = 0x0;
    cs_press_iic_write(reg_addr, reg_data, len);
    return ret;
}

static int cs_proc_get_rawdata_open(struct inode *inode, struct file *filp)
{
    if(filp == NULL){
        return -1;
    }
    return single_open(filp, cs_proc_get_rawdata_show, NULL);
}
/**
  * @brief      check calibration result
  * @param[out] check result data buf
  * @retval     0:running, -1: error, 1: success, 2: fail, 3: overtime
  */
static int cs_proc_get_forcedata_show(struct seq_file *m,void *v)
{
    int ret;
    unsigned char reg_addr;
    unsigned char reg_data[64];
    int each_raw_size = 2;
    int len = 16;
    short raw_data[16];
    int i;
    int retry = 50;

    cs_press_wakeup_iic();

    set_debug_mode(DEBUG_MODE_REG, DEBUG_FORCE_DATA_MODE);

    do {
        msleep(5);
        len = get_debug_data_ready((unsigned char)DEBUG_READY_REG);
        if (len > 0) {
            ret = get_debug_data(DEBUG_DATA_REG, reg_data, len);
            seq_printf(m,"D0 : %d, D1 : %d, D2 : %d, len : %d\n", reg_data[0], reg_data[1], reg_data[2], len);
        }
        retry--;
    } while (len == 0 && retry > 0);

    ret = 0;
    if (len > 0)
    {
        for (i = 0 ; i < CH_NUM ; i++)
        {
            raw_data[i] = ((short)(reg_data[i * each_raw_size] & 0xff)
                | ((short)(reg_data[i * each_raw_size + 1] & 0xff) << 8));

            seq_printf(m, "diff_data:%05d ", raw_data[i]);
        }
        seq_printf(m, "\n");
    } else {
        seq_printf(m, "Data is not ready yet!\n");
    }
    set_debug_mode(DEBUG_MODE_REG, DEBUG_CLEAR_MODE);
    reg_addr = DEBUG_READY_REG;
    len = 1;
    reg_data[0] = 0x0;
    cs_press_iic_write(reg_addr, reg_data, len);
    return 0;
}

static int cs_proc_get_forcedata_open(struct inode *inode, struct file *filp)
{
    if(filp == NULL){
        LOG_ERR("filp is null\n");
        return -1;
    }
    return single_open(filp,cs_proc_get_forcedata_show,NULL);
}

static ssize_t cs_proc_left_press_level_write(struct file *file, const char __user *buf,
                                            size_t count, loff_t *offset)
{
    char ret = 0;
    const char *startpos = NULL;
    int i;
    int tempdata = 0;
    char *kbuf = NULL;
    int err = 0;
    unsigned char read_temp[FW_ONE_BLOCK_LENGTH_R] = {0};

    if((count <= 0)||(count > 5))
    {
        LOG_ERR("argument err\n");
        goto exit_flag;
    }
    kbuf = kzalloc(count, GFP_KERNEL );
    if (!kbuf) {
        goto exit_kfree;
    }
    if (copy_from_user(kbuf, buf, count)) {
        goto exit_kfree;
    }

    startpos = kbuf;

    err = kstrtouint(startpos, 10, &tempdata);
    if (err) {
        LOG_ERR("err kstrtouint\n");
        goto exit_kfree;
    }
    cs_press_wakeup_iic();

    ret = cs_press_iic_read(AP_RW_BUTTON_FORCE_THR_REG, read_temp, CS_FORCE_TRIG_LENGTH);  /* read force trig */
    if(ret == 0)
    {
        read_temp[0] = tempdata&0xff;     /*set left force tig value*/
        read_temp[1] = (tempdata>>8)&0xff;
    }else{
        LOG_ERR("err read force\n");
        goto exit_kfree;
    }
    read_temp[4] = 0x01;  /*set left only*/
    read_temp[5] = 0;
    for(i=0; i<5; i++)
    {
        read_temp[5] += read_temp[i];
    }
    read_temp[5] = (0xFF - read_temp[5]) + 1;

    ret = cs_press_iic_write(AP_RW_BUTTON_FORCE_THR_REG, read_temp, CS_FORCE_TRIG_LENGTH);
    if(ret < 0)
    {
        LOG_ERR("write left press level:failed\n");
    }else{
        LOG_DEBUG("write left press level:ok\n");
    }
exit_kfree:
    kfree(kbuf);
exit_flag:
    return count;
}

static ssize_t cs_proc_right_press_level_write(struct file *file, const char __user *buf,
                                            size_t count, loff_t *offset)
{
    char ret = 0;
    const char *startpos = NULL;
    int i;
    int tempdata = 0;
    char *kbuf = NULL;
    int err = 0;
    unsigned char read_temp[FW_ONE_BLOCK_LENGTH_R] = {0};

    if((count <= 0)||(count > 5))
    {
        LOG_ERR("argument err\n");
        goto exit_flag;
    }
    kbuf = kzalloc(count, GFP_KERNEL );
    if (!kbuf) {
        goto exit_kfree;
    }
    if (copy_from_user(kbuf, buf, count)) {
        goto exit_kfree;
    }

    startpos = kbuf;

    err = kstrtouint(startpos, 10, &tempdata);
    if (err) {
        LOG_ERR("err kstrtouint\n");
        goto exit_kfree;
    }
    cs_press_wakeup_iic();
    ret = cs_press_iic_read(AP_RW_BUTTON_FORCE_THR_REG, read_temp, CS_FORCE_TRIG_LENGTH);  /* read force trig */
    if(ret == 0)
    {
        read_temp[2] = tempdata&0xff;
        read_temp[3] = (tempdata>>8)&0xff;
    }else{
        LOG_ERR("err read force\n");
        goto exit_kfree;
    }
    read_temp[4] = 0x02;  /*set right only*/
    read_temp[5] = 0;
    for(i = 0; i < 5; i++)
    {
        read_temp[5] += read_temp[i];
    }
    read_temp[5] = (0xFF - read_temp[5]) + 1;

    ret = cs_press_iic_write(AP_RW_BUTTON_FORCE_THR_REG, read_temp, CS_FORCE_TRIG_LENGTH);
    if(ret < 0)
    {
        LOG_ERR("write right press level:failed\n");
    }else{
        LOG_DEBUG("write right press level:ok\n");
    }
exit_kfree:
    kfree(kbuf);
exit_flag:
    return count;

}

static int cs_proc_read_left_press_level_show(struct seq_file *m,void *v)
{
    char ret = 0;
    int i;
    int force_trig;
    unsigned char read_temp[FW_ONE_BLOCK_LENGTH_R] = {0};
    unsigned char check_sum = 0;
    cs_press_wakeup_iic();
    ret = cs_press_iic_read(AP_RW_BUTTON_FORCE_THR_REG, read_temp, CS_FORCE_TRIG_LENGTH);  /* read force/release trig */
    for(i = 0; i < CS_FORCE_TRIG_LENGTH; i++)
    {
        check_sum += read_temp[i];
    }
    if((ret == 0)&&(check_sum == 0))
    {
        force_trig = ((unsigned short)read_temp[1] << 8) | read_temp[0];
        seq_printf(m,"%d\n", force_trig);
    }else{
        seq_printf(m,"read L trig err\n");
        ret = -1;
    }
    return ret;
}
static int cs_proc_read_left_press_level_open(struct inode *inode, struct file *filp)
{
    return single_open(filp,cs_proc_read_left_press_level_show,NULL);
}


static int cs_proc_read_right_press_level_show(struct seq_file *m,void *v)
{
    char ret = 0;
    int i;
    int force_trig;
    unsigned char read_temp[FW_ONE_BLOCK_LENGTH_R] = {0};
    unsigned char check_sum = 0;
    cs_press_wakeup_iic();
    ret = cs_press_iic_read(AP_RW_BUTTON_FORCE_THR_REG, read_temp, CS_FORCE_TRIG_LENGTH);  /* read force/release trig */
    for(i = 0; i < CS_FORCE_TRIG_LENGTH; i++)
    {
        check_sum += read_temp[i];
    }
    if((ret == 0)&&(check_sum == 0))
    {
        force_trig = ((unsigned short)read_temp[3] << 8) | read_temp[2];
        seq_printf(m,"%d\n", force_trig);
    }else{
        seq_printf(m,"read R trig err\n");
        ret = -1;
    }
    return ret;
}

static int cs_proc_read_right_press_level_open(struct inode *inode, struct file *filp)
{
    return single_open(filp,cs_proc_read_right_press_level_show,NULL);
}

static int cs_proc_calibration_param_show(struct seq_file *m,void *v)
{
    int ret = 0;
    int read_coeffs[CH_NUM];
    int i;
    unsigned char read_temp[FW_ONE_BLOCK_LENGTH_R] = {0};

    cs_press_wakeup_iic();
    ret |= cs_press_iic_read(AP_RD_CAIL_REG, read_temp, CS_CALI_PARA_LENGTH);  /* calibrate coef */

    if (ret == 0)
    {
        for (i = 0 ; i < CH_NUM ; i++)
        {
            read_coeffs[i] = ((unsigned short)read_temp[i*2+0]<<8) | read_temp[i*2+1];
            seq_printf(m, "%05d ", read_coeffs[i]);
        }
        seq_printf(m, "\n");
    } else {
        seq_printf(m, "read coefs fail!\n");
    }
    return 0;
}

static int cs_proc_calicoef_open(struct inode *inode, struct file *filp)
{
    if(filp == NULL){
        return -1;
    }
    return single_open(filp, cs_proc_calibration_param_show, NULL);
}


/**
  * @brief    check iic function
  * @param
  * @retval
*/
int cs_check_i2c(void)
{
    int retry = 3;
    unsigned char rbuf[2];
    unsigned char addr;
    int len = 0;

    addr = 0x03;
    len = 1;
    rbuf[0] = 0;
    rbuf[1] = 0;

    do {
        if (cs_press_iic_rw_test(0x67) >= 0)
            return 1;
        msleep(50);
        retry--;
        LOG_INFO("read fw ver fail!retry:%d\n", retry);
    } while (retry > 0);

    retry = 3;
    do {
        cs_press_reset_ic();
        msleep(300);
        if (cs_press_iic_rw_test(0x67) >= 0)
            return 1;
        retry--;
        LOG_INFO("reset fw fail!retry:%d\n", retry);
    } while (retry > 0);

    return -1;
}

/**
  * @brief    update function
  * @param
  * @retval
*/

static void update_work_func(struct work_struct *worker)
{
    int ret;
    ret = cs_press_init();
    if (ret < 0){
        LOG_ERR("press init err\n");
    }else{
        LOG_DEBUG("press init ok\n");
    }
    cancel_delayed_work(&g_cs_press.update_worker);
}
/*
#ifdef I2C_CHECK_SCHEDULE

static void  i2c_check_func(struct work_struct *worker)
{
    int ret = 0;
    ret = cs_check_i2c();
    if(ret != -1){
        schedule_delayed_work(&g_cs_press.i2c_check_worker, msecs_to_jiffies(CHECK_DELAY_TIME));
        LOG_ERR("i2c_check_func start,delay 20s.\n");
    }

}
#endif
*/
#ifndef ALIENTEK
static void shell_temp_work_func(struct work_struct *worker)
{
    char ret = 0;
    unsigned char i;
    int shell_temp = 0;
    unsigned char write_buf[FW_ONE_BLOCK_LENGTH_R] = {0};

    if(g_cs_press.cs_shell_themal_enable&&g_cs_press.cs_shell_themal_init_flag)
    {
        ret = cs_press_get_shell_temp(&shell_temp);
        if(ret >= 0)
        {
            if(shell_temp < -850)
            {
                shell_temp = -850;
            }
            if(shell_temp > 850)
            {
                shell_temp = 850;
            }
            write_buf[0] = shell_temp&0xff;
            write_buf[1] = (shell_temp>>8)&0xff;
            write_buf[2] = 0x01;
            write_buf[3] = 0;
            for(i = 0; i < 3; i++)
            {
                write_buf[3] += write_buf[i];
            }
            write_buf[3] = (0xFF - write_buf[3]) + 1;

            ret = cs_press_iic_write(AP_RW_SHEll_TEMP_REG, write_buf, CS_SHELL_TEMP_LENGTH);
            if(ret < 0)
            {
                LOG_ERR("write shell temp failed\n");
            }else{
                LOG_DEBUG("write shell temp ok\n");
            }
        }
        schedule_delayed_work(&g_cs_press.shell_temp_worker, msecs_to_jiffies(SHELL_TEMP_DELAY_TIME));
    }else{
        cancel_delayed_work(&g_cs_press.shell_temp_worker);
    }
}
#endif

/* procfs api*/
static int cs_proc_fw_info_show(struct seq_file *m,void *v)
{
    char ret = 0;
    unsigned char read_temp[FW_ONE_BLOCK_LENGTH_R] = {0};
    if(get_device_updating_flag()){
        LOG_ERR("fw updating exit\n");
        seq_printf(m,"fw updating\n");
        return 0;
    }

    cs_press_wakeup_iic();
    ret |= cs_press_iic_read(AP_VERSION_REG, read_temp, CS_FW_VERSION_LENGTH);  /*FW Version*/
    if(ret==0){
        seq_printf(m,"ic: CSA37F71\nfw_ver: %d %d %d %d\n",
             read_temp[0], read_temp[1], read_temp[2], read_temp[3]);
    }else{
        seq_printf(m,"read fw info err\n");
    }
    return 0;
}

static int cs_proc_fw_info_open(struct inode *inode, struct file *filp)
{
    if(filp == NULL){

        return -1;
    }
    return single_open(filp,cs_proc_fw_info_show,NULL);
}


static int cs_proc_local_fw_info_show(struct seq_file *m,void *v)
{
    char ret = 0;
    const struct firmware *fw = NULL;
    //unsigned char *fw_array;

    ret = request_firmware(&fw, FW_FILE_NAME, &(g_cs_press.client->dev));
    if (!ret && fw) {
        //fw_array = fw->data;
        seq_printf(m,"ic: CSA37F71\nfw_ver: %u %u %u %u\n",
             fw->data[FW_ADDR_VERSION-2], fw->data[FW_ADDR_VERSION-1], fw->data[FW_ADDR_VERSION], fw->data[FW_ADDR_VERSION+1]);
    } else {
        LOG_ERR("read fw info err, ret=%d\n", ret);
        seq_printf(m,"read fw info err\n");
    }

    if (fw != NULL) {
        release_firmware(fw);
    }
    return 0;
}

static int cs_proc_local_fw_info_open(struct inode *inode, struct file *filp)
{
    if(filp == NULL){

        return -1;
    }
    return single_open(filp,cs_proc_local_fw_info_show,NULL);
}

static ssize_t cs_proc_fw_file_update_write(struct file *file, const char __user *buf,
                                            size_t count, loff_t *offset)
{
    char *kbuf = NULL;
    int err = 0;

    kbuf = kzalloc(count, GFP_KERNEL );
    if (!kbuf) {
        goto exit;
    }
    if (copy_from_user(kbuf, buf, count)) {
        goto exit_kfree;
    }
    g_cs_press.cs_shell_themal_enable = 0;
    if(kbuf[0] == '2')
    {
        g_cs_press.update_type = FORCE_FILE_UPDATE;
    }else
    {
        g_cs_press.update_type = HIGH_VER_FILE_UPDATE;
    }
    err = fml_fw_update_by_file();
    if (err == 0)
        LOG_DEBUG("pass!\n");
    else
        LOG_ERR("%d,failed!\n", err);

exit_kfree:
    kfree(kbuf);
exit:
    return count;
}

static int cs_read_boot_version_show(struct seq_file *m, void *v)
{
    char result = 0;

    unsigned char boot_ver_buf[4];

    result = cs_read_boot_version(boot_ver_buf);
    if (result == 0){
        seq_printf(m,"%02X %02X %02X %02X\n", boot_ver_buf[0],boot_ver_buf[1],boot_ver_buf[2],boot_ver_buf[3]);
    }else{
        seq_printf(m,"ERR\n");
    }
    return 0;
}

#define CS_BOOT_VERSION_HEADER  0x71
static int cs_auto_test_show(struct seq_file *m, void *v)
{
    unsigned char reg_rw[FW_ONE_BLOCK_LENGTH_R] = { 0 };
    int i = 0;
    int retry = 20;
    int result = -1;
    uint8_t len = 0;
    uint16_t status = 0;
    char ret = 0 ;
/*
    unsigned char boot_ver_buf[4];

    result = cs_read_boot_version(boot_ver_buf);

    LOG_INFO("boot_ver: %02X %02X %02X %02X\n", boot_ver_buf[0], boot_ver_buf[1], boot_ver_buf[2], boot_ver_buf[3]);
*/

    memset(reg_rw, 0x00, FW_ONE_BLOCK_LENGTH_R);
    ret = cs_press_iic_write(0xFB, reg_rw, 1);
    if (ret < 0) {
        LOG_ERR("IIC write 0xFB 0x%02x failed!!\n", reg_rw[0]);
        goto exit;
    }

    memset(reg_rw, 0x00, FW_ONE_BLOCK_LENGTH_R);
    ret = cs_press_iic_write(0xFC, reg_rw, 1);
    if (ret < 0) {
        LOG_ERR("IIC write 0xFC 0x%02x failed!!\n", reg_rw[0]);
        goto exit;
    }

    memset(reg_rw, 0x00, FW_ONE_BLOCK_LENGTH_R);
    reg_rw[0] = 0xA4;

    ret = cs_press_iic_write(0xFB, reg_rw, 1);
    if (ret < 0) {
        LOG_ERR("IIC write 0xFB 0x%02x failed!!\n", reg_rw[0]);
        goto exit;
    }

    memset(reg_rw, 0x00, FW_ONE_BLOCK_LENGTH_R);
    while (!reg_rw[0] && retry) {
        retry--;
        cs_press_delay_ms(2);
        ret = cs_press_iic_read(0xFC, reg_rw, 1);
        if (ret < 0) {
            LOG_ERR("%s: IIC read 0xFC failed!!\n", __func__);
            goto exit;
        }
        LOG_INFO("read 0xFC 0x%02x\n", reg_rw[0]);
    }
    len = (uint8_t)reg_rw[0];
    if (!len || (len > FW_ONE_BLOCK_LENGTH_R)) {
        LOG_ERR("%s: len=%u invalid!!\n", __func__, len);
        goto exit;
    }
    memset(reg_rw, 0x00, FW_ONE_BLOCK_LENGTH_R);
    ret = cs_press_iic_read(0xFD, reg_rw, len);
    if (ret < 0) {
        LOG_ERR("%s: IIC read 0xFD failed!!\n", __func__);
        goto exit;
    }
    status = (uint16_t)(reg_rw[0] + (reg_rw[1] << 8));
    LOG_INFO("channel broken status: 0x%02x\n", status);
    for (i = 1; i < len / 2 - 1; i++) {
        LOG_DEBUG("word[%d]: 0x%02x\n", i, (uint16_t)(reg_rw[i * 2] + (reg_rw[i * 2 + 1] << 8)));
    }
    for (i = 0; i < CH_COUNT; i++) {
        if (status & (0x01 << i)) {
            LOG_ERR("auto test FAILED, channel %d broken!!\n", i + 1);
            goto exit;
        }
    }
    LOG_INFO("auto test PASS\n");
    result = 0;

    memset(reg_rw, 0x00, FW_ONE_BLOCK_LENGTH_R);
    ret = cs_press_iic_write(0xFB, reg_rw, 1);
    if (ret < 0) {
        LOG_ERR("IIC write 0xFB 0x%02x failed!!\n", reg_rw[0]);
    }

    memset(reg_rw, 0x00, FW_ONE_BLOCK_LENGTH_R);
    ret = cs_press_iic_write(0xFC, reg_rw, 1);
    if (ret < 0) {
        LOG_ERR("IIC write 0xFC 0x%02x failed!!\n", reg_rw[0]);
    }
exit:
    if (result == 0){
        seq_printf(m, "OK\n");
    }else{
        seq_printf(m, "NG\n");
    }
    return 0;
}

static ssize_t cs_proc_left_press_gear_write(struct file *file, const char __user *buf,
        size_t count, loff_t *offset) 
{
    const char *startpos = NULL;
    int tempdata = 0;
    char kbuf[128] = { 0 };
    int err = 0;
    struct scene_client_t get_scene_client;

    if(count > 127){
        count = 127;
    }

    if (copy_from_user(kbuf, buf, count)) {
        LOG_ERR("copy_from_user error\n");
        return -1;
    }

    startpos = kbuf;

    err = kstrtouint(startpos, 16, &tempdata);
    if (err) {
        LOG_ERR("err kstrtouint\n");
        return -1;
    }

    if (tempdata < 1 || tempdata > 3) {
        LOG_ERR("left_press_gear invalid %d\n", tempdata);
        return -1;
    }

    sprintf(kbuf, "com.oplus.game|scene-game|-1|%d|-1", tempdata);

    if(parse_sence_str(kbuf, &get_scene_client) != 0){
        LOG_DEBUG("parse str err\n");
        return -1;
    }

    cs_insert_scene_client(get_scene_client);

    return count;
}

static ssize_t cs_proc_right_press_gear_write(struct file *file, const char __user *buf,
        size_t count, loff_t *offset) 
{
    const char *startpos = NULL;
    int tempdata = 0;
    char kbuf[128] = { 0 };
    int err = 0;
    struct scene_client_t get_scene_client;

    if(count > 127){
        count = 127;
    }

    if (copy_from_user(kbuf, buf, count)) {
        LOG_ERR("copy_from_user error\n");
        return -1;
    }

    startpos = kbuf;

    err = kstrtouint(startpos, 16, &tempdata);
    if (err) {
        LOG_ERR("err kstrtouint\n");
        return -1;
    }

    if (tempdata < 1 || tempdata > 3) {
        LOG_ERR("right_press_gear invalid %d\n", tempdata);
        return -1;
    }

    sprintf(kbuf, "com.oplus.game|scene-game|-1|-1|%d", tempdata);

    if(parse_sence_str(kbuf, &get_scene_client) != 0){
        LOG_DEBUG("parse str err\n");
        return -1;
    }

    cs_insert_scene_client(get_scene_client);

    return count;
}

static ssize_t cs_proc_game_switch_write(struct file *file, const char __user *buf,
                                            size_t count, loff_t *offset)
{
    const char *startpos = NULL;
    int tempdata = 0;
    char kbuf[128] = { 0 };
    int err = 0;
    struct scene_client_t get_scene_client;

    if (copy_from_user(kbuf, buf, count)) {
        LOG_ERR("copy_from_user error\n");
        return -1;
    }

    startpos = kbuf;

    err = kstrtouint(startpos, 16, &tempdata);
    if (err) {
        LOG_ERR("err kstrtouint\n");
        return -1;
    }

    if (tempdata < 0 || tempdata > 1) {
        LOG_ERR("game_switch invalid %d\n", tempdata);
        return -1;
    }

    sprintf(kbuf, "com.oplus.game|scene-game|%d|-1|-1", tempdata);

    if(parse_sence_str(kbuf, &get_scene_client) != 0){
        LOG_DEBUG("parse str err\n");
        return -1;
    }

    cs_insert_scene_client(get_scene_client);

    return count;
}
char cs_press_get_shell_temp(int *shell_temp)
{
    int temp_val = 0, rc = -1;
#ifndef ALIENTEK
    rc = thermal_zone_get_temp(g_cs_press.cs_thermal_zone_device, &temp_val);
#endif
    if (rc) {
        LOG_ERR("thermal_zone_get_temp get error\n");
        return -1;
    }else{
        *shell_temp = temp_val / 100;
        return 0;
    }
}

static ssize_t cs_proc_shell_temp_write(struct file *file, const char __user *buf,
                                            size_t count, loff_t *offset)
{
    char ret = 0;
    const char *startpos = NULL;
    int i;
    int tempdata = 0;
    char *kbuf = NULL;
    int err = 0;
    unsigned char write_buf[FW_ONE_BLOCK_LENGTH_R] = {0};

    if((count <= 0)||(count > 4))
    {
        LOG_ERR("argument err\n");
        goto exit_flag;
    }
    kbuf = kzalloc(count, GFP_KERNEL );
    if (!kbuf) {
        goto exit_kfree;
    }
    if (copy_from_user(kbuf, buf, count)) {
        goto exit_kfree;
    }

    startpos = kbuf;

    err = kstrtouint(startpos, 10, &tempdata);
    if (err) {
        LOG_ERR("err kstrtouint\n");
        goto exit_kfree;
    }
    cs_press_wakeup_iic();
    if(tempdata < -850)
    {
        tempdata = -850;
    }
    if(tempdata > 850)
    {
        tempdata = 850;
    }
    write_buf[0] = tempdata&0xff;
    write_buf[1] = (tempdata>>8)&0xff;
    write_buf[2] = 0x01;
    write_buf[3] = 0;
    for(i = 0; i < 3; i++)
    {
        write_buf[3] += write_buf[i];
    }
    write_buf[3] = (0xFF - write_buf[3]) + 1;

    ret = cs_press_iic_write(AP_RW_SHEll_TEMP_REG, write_buf, CS_SHELL_TEMP_LENGTH);
    if(ret < 0)
    {
        LOG_ERR("write shell temp failed\n");
    }else{
        LOG_DEBUG("write shell temp ok\n");
    }
exit_kfree:
    kfree(kbuf);
exit_flag:
    return count;
}

static ssize_t cs_proc_power_gpio_write(struct file *file, const char __user *buf,
                                            size_t count, loff_t *offset)
{
    const char *startpos = NULL;
    int tempdata = 0;
    char *kbuf = NULL;
    int err = 0;

    if((count <= 0)||(count > 4))
    {
        LOG_ERR("argument err\n");
        goto exit_flag;
    }
    kbuf = kzalloc(count, GFP_KERNEL );
    if (!kbuf) {
        goto exit_kfree;
    }
    if (copy_from_user(kbuf, buf, count)) {
        goto exit_kfree;
    }

    startpos = kbuf;

    err = kstrtouint(startpos, 10, &tempdata);
    if (err) {
        LOG_ERR("err kstrtouint\n");
        goto exit_kfree;
    }

    if(gpio_is_valid(g_cs_press.power_gpio))
    {
        gpio_set_value(g_cs_press.power_gpio, (tempdata)? POWER_GPIO_HIGH:POWER_GPIO_LOW);

    }else{
        LOG_ERR("gpio power is invalid\n");
    }

exit_kfree:
    kfree(kbuf);
exit_flag:
    return count;
}

static ssize_t cs_proc_shell_temp_en_write(struct file *file, const char __user *buf,
                                            size_t count, loff_t *offset)
{
    const char *startpos = NULL;
    int tempdata = 0;
    char *kbuf = NULL;
    int err = 0;

    if((count <= 0)||(count > 4))
    {
        LOG_ERR("argument err\n");
        goto exit_flag;
    }
    kbuf = kzalloc(count, GFP_KERNEL );
    if (!kbuf) {
        goto exit_kfree;
    }
    if (copy_from_user(kbuf, buf, count)) {
        goto exit_kfree;
    }

    startpos = kbuf;

    err = kstrtouint(startpos, 16, &tempdata);
    if (err) {
        LOG_ERR("err kstrtouint\n");
        goto exit_kfree;
    }
    if(tempdata == 0)
    {
        g_cs_press.cs_shell_themal_enable = 0;
    }
    if(tempdata == 1)
    {
        g_cs_press.cs_shell_themal_enable = 1;
        schedule_delayed_work(&g_cs_press.shell_temp_worker, msecs_to_jiffies(SHELL_TEMP_DELAY_TIME));
    }
exit_kfree:
    kfree(kbuf);
exit_flag:
    return count;
}

static ssize_t cs_proc_ndt_debug_int_write(struct file *file, const char __user *buf,
                                            size_t count, loff_t *offset)
{
    const char *startpos = NULL;
    int tempdata = 0;
    char *kbuf = NULL;
    int err = 0;

    if((count <= 0)||(count > 4))
    {
        LOG_ERR("argument err\n");
        goto exit_flag;
    }
    kbuf = kzalloc(count, GFP_KERNEL );
    if (!kbuf) {
        goto exit_kfree;
    }
    if (copy_from_user(kbuf, buf, count)) {
        goto exit_kfree;
    }

    startpos = kbuf;

    err = kstrtouint(startpos, 16, &tempdata);
    if (err) {
        LOG_ERR("err kstrtouint\n");
        goto exit_kfree;
    }
    if(tempdata == 0)
    {
        cs_press_clean_debugmode();
    }
    if(tempdata == 1)
    {
        cs_press_read_ndt_debug_init();
    }
exit_kfree:
    kfree(kbuf);
exit_flag:
    return count;
}


static ssize_t cs_proc_gpio_toggle_write(struct file *file, const char __user *buf,
                                            size_t count, loff_t *offset)
{
    const char *startpos = NULL;
    int tempdata = 0;
    char *kbuf = NULL;
    int err = 0;

    if((count <= 0)||(count > 4))
    {
        LOG_ERR("argument err\n");
        goto exit_flag;
    }
    kbuf = kzalloc(count, GFP_KERNEL );
    if (!kbuf) {
        goto exit_kfree;
    }
    if (copy_from_user(kbuf, buf, count)) {
        goto exit_kfree;
    }

    startpos = kbuf;

    err = kstrtouint(startpos, 16, &tempdata);
    if (err) {
        LOG_ERR("err kstrtouint\n");
        goto exit_kfree;
    }
    if(tempdata == 0)
    {
        if(cs_press_gpio_toggle(0) == 0)/*turn off toggle*/
        {
            LOG_ERR("gpio_toggle off ok\n");
        }else{
            LOG_ERR("gpio_toggle off fail\n");
        }
    }
    if(tempdata == 1)
    {
        if(cs_press_gpio_toggle(1) == 0)/*turn on toggle*/
        {
            LOG_ERR("gpio_toggle on ok\n");
        }else{
            LOG_ERR("gpio_toggle on fail\n");
        }
    }
exit_kfree:
    kfree(kbuf);
exit_flag:
    return count;
}


/*********
*
* input format: cilent|scene|active|L-level|R-level
*
**********/
int parse_sence_str(char* parse_str,  struct scene_client_t *out_struct)
{
    const char *delims = "|";
    char *result = NULL;
    int temp_data;
    struct scene_para_t *ptemp_list = NULL;

    result = strsep(&parse_str, delims);
    if(result != NULL){
        strncpy(out_struct->client_name, result, MIN(strlen(result)+1, NAME_MAX_LENS));
    }else{
        LOG_INFO("parse client_name err\n");
        return -1;
    }
    out_struct->pscene = NULL;
    /**********add scene********/
    result = strsep(&parse_str, delims);
    if(result != NULL){
        list_for_each_entry(ptemp_list, &gScenes, node){
            if(ptemp_list){
                 if(0 == strcmp(ptemp_list->scene_name, result)){
                    out_struct->pscene = ptemp_list;
                }
            }
        }
    }else{
        LOG_INFO("parse scene_name err\n");
        return -2;
    }

    result = strsep(&parse_str, delims);
    if(result != NULL){
        if(kstrtoint(result, 10, &temp_data)){
            LOG_INFO("str to int err\n");
            return -3;
        }
        out_struct->state = temp_data;
    }else{
        LOG_INFO("parse state err\n");
        return -3;
    }

    result = strsep(&parse_str, delims);
    if(result != NULL){
        if(kstrtoint(result, 10, &temp_data)){
            LOG_INFO("str to int err\n");
            return -3;
        }
        out_struct->left_geat_level = temp_data;
    }else{
        LOG_INFO("parse left_geat_level err\n");
        return -4;
    }

    result = strsep(&parse_str, delims);
    if(result != NULL){
        if(kstrtoint(result, 10, &temp_data)){//error
            LOG_INFO("str to int err\n");
            return -3;
        }
        out_struct->right_geat_level = temp_data;
    }else{
        LOG_INFO("parse right_geat_level err\n");
        return -5;
    }
    if(parse_str != NULL){
        LOG_INFO("%s,str too much\n",parse_str);
        return -6;
    }
    return 0;
}

/*********
* sample_mode: 0-ftm, 1-1Hz, 2-100Hz, 3-166Hz
* left_geat --int left_geat[PRESS_LEVEL_NUM];
* right_geat --int right_geat[PRESS_LEVEL_NUM];
**********/
int set_device_run_mode(int sample_mode, int left_geat, int right_geat)
{
    int ret = 0;
    unsigned char retry = RETRY_NUM;
    unsigned char write_buf[10] = {0};
    unsigned char read_buf[10] = {0};

    switch(sample_mode)
    {
        case GAME_MODE:/*game 166Hz*/
            write_buf[0] = 0x03;
            break;
        case NORMAL_MODE:/*setting&camera 100Hz*/
            write_buf[0] = 0x02;
            break;
        case SLEEP_MODE:/*sleep 1Hz*/
            write_buf[0] = 0x01;
            break;
        default:/*error*/
            LOG_ERR("command err\n");
            return -1;
            break;
    }
    write_buf[1] = 0x01;/*set write flag*/
    write_buf[2] = 0 - check_sum(write_buf, 2);

    do{
        if(ret != 0){
            cs_press_delay_ms(1);
        }
        ret = cs_press_iic_write(AP_RW_WORK_MODE_REG, write_buf, 3);
        if(ret == 0){
            ret = cs_press_iic_read(AP_RW_WORK_MODE_REG, read_buf, 3);
            if(ret == 0){
                if(check_sum(read_buf,3) != 0){
                    LOG_ERR("read mode checksum err\n");
                    ret = -1;
                }else{
                    if(write_buf[0] != read_buf[0]){
                        LOG_ERR("read mode compare err\n");
                        ret = -1;
                    }
                }
            }
        }
    }while((ret != 0 ) && (retry--));

    if(ret < 0)
    {
        LOG_ERR("switch mode failed\n");
    }else{
        switch(sample_mode)
        {
            case GAME_MODE:/*game 166Hz*/
                LOG_DEBUG("game switch ok\n");
                break;
            case NORMAL_MODE:/*setting&camera 100Hz*/
                LOG_DEBUG("normal switch ok\n");
                break;
            case SLEEP_MODE:/*sleep 1Hz*/
                LOG_DEBUG("sleep switch ok\n");
                break;
        }
        if((left_geat > 0)&&(right_geat > 0)){
            cs_set_press_threshold(left_geat, right_geat);
        }
        /*game mode*/
        if(write_buf[0] == 0x03)
        {
            g_cs_press.cs_shell_themal_enable = 1;
            #ifndef ALIENTEK
            schedule_delayed_work(&g_cs_press.shell_temp_worker, msecs_to_jiffies(SHELL_TEMP_DELAY_TIME));
            #endif
        }else{
            g_cs_press.cs_shell_themal_enable = 0;
        }
    }
    return ret;
}

/**********
 * 
 * 
**********/
void set_mode_from_client_list(struct work_struct *worker)
{
    struct scene_client_t *temp_scene_client = NULL;
    struct scene_client_t *pclient = NULL;
    struct scene_client_t *pclient_bak;
    unsigned char left_geat_level, right_geat_level;

    list_for_each_entry_safe(temp_scene_client, pclient_bak, &gSceneClients, node)
    {
        LOG_DEBUG("m: %s|%s|%d|%d|%d\n",temp_scene_client->client_name,temp_scene_client->pscene->scene_name,\
                    temp_scene_client->state,temp_scene_client->left_geat_level,temp_scene_client->right_geat_level);
        if(temp_scene_client->state){
            if (pclient == NULL){
                pclient = temp_scene_client;
            }else{
                pclient = temp_scene_client->pscene->priority > pclient->pscene->priority ? temp_scene_client : pclient;
            }
        }
    }

    if (pclient != NULL){
        LOG_DEBUG("action:%s|%s\n",pclient->client_name, pclient->pscene->scene_name);

        /***********pclient->xx_geat_level start from 1**************/
        left_geat_level = (pclient->left_geat_level > PRESS_LEVEL_NUM)? PRESS_LEVEL_NUM : pclient->left_geat_level;
        left_geat_level = (pclient->left_geat_level <= LOW)? 1 : pclient->left_geat_level;

        right_geat_level = (pclient->right_geat_level > PRESS_LEVEL_NUM)? PRESS_LEVEL_NUM : pclient->right_geat_level;
        right_geat_level = (pclient->right_geat_level <= LOW)? 1 : pclient->right_geat_level;
        /*charge mode*/
        if(pclient->pscene->sample_mode == GAME_MODE){
            if(cs_read_charge_state()){
                LOG_DEBUG("charge mode\n");
                set_device_run_mode(pclient->pscene->sample_mode, \
                                pclient->pscene->left_geat[(enum PRESS_LEVEL)(left_geat_level-1)], \
                                pclient->pscene->charge_right_geat[(enum PRESS_LEVEL)(right_geat_level-1)]);
            }else{
                LOG_DEBUG("no charge mode\n");
                set_device_run_mode(pclient->pscene->sample_mode, \
                                pclient->pscene->left_geat[(enum PRESS_LEVEL)(left_geat_level-1)], \
                                pclient->pscene->right_geat[(enum PRESS_LEVEL)(right_geat_level-1)]);
            }
        }
    }else{
        set_device_run_mode(1,0,0);
    }
}

void scene_client_copy(struct scene_client_t *dst_lient , struct scene_client_t *src_lient)
{
    if((dst_lient == NULL) || (src_lient == NULL)) {
         return ;
    }

    memcpy(dst_lient->client_name, src_lient->client_name, NAME_MAX_LENS);
    dst_lient->pscene = src_lient->pscene;
    // -1 no update params
    dst_lient->state = src_lient->state != -1 ? src_lient->state : dst_lient->state;
    dst_lient->left_geat_level = src_lient->left_geat_level != -1 ? src_lient->left_geat_level : dst_lient->left_geat_level;
    dst_lient->right_geat_level = src_lient->right_geat_level != -1 ? src_lient->right_geat_level : dst_lient->right_geat_level;
}

void scene_para_copy(struct scene_para_t*dst_scene, struct scene_para_t*src_scene)
{
    if((dst_scene == NULL)||(src_scene == NULL)){
           return ;
    }
    memcpy(dst_scene->scene_name, src_scene->scene_name, NAME_MAX_LENS);
    memcpy(dst_scene->left_geat, src_scene->left_geat, sizeof(src_scene->left_geat));
    memcpy(dst_scene->right_geat, src_scene->right_geat, sizeof(src_scene->right_geat));
    memcpy(dst_scene->charge_right_geat, src_scene->charge_right_geat, sizeof(src_scene->charge_right_geat));

    dst_scene->sample_mode = src_scene->sample_mode ;
    dst_scene->priority = src_scene->priority ;
    dst_scene->left_geat_num = src_scene->left_geat_num ;
    dst_scene->right_geat_num = src_scene->right_geat_num ;
}


char is_scene_client_invalid(struct scene_client_t scene_client)
{

    if(strlen(scene_client.client_name) == 0){
        LOG_ERR("client_name is NULL\n");
        return -1;
    }

    if(scene_client.pscene == NULL){
        LOG_ERR("scene name is err\n");
        return -2;
    }

    if((scene_client.state != 0)&&(scene_client.state != 1)){
        LOG_ERR("state=%d exceed\n",scene_client.state);
        return -3;
    }

    if((scene_client.left_geat_level > PRESS_LEVEL_NUM)||(scene_client.left_geat_level < 1)){
        LOG_ERR("left_geat_level=%d exceed\n",scene_client.left_geat_level);
        return -4;
    }

    if((scene_client.right_geat_level > PRESS_LEVEL_NUM)||(scene_client.right_geat_level < 1)){
        LOG_ERR("right_geat_level=%d exceed\n",scene_client.right_geat_level);
        return -5;
    }
    return 0;
}

static int cs_insert_scene_client(struct scene_client_t get_scene_client)
{
    struct scene_client_t *temp_scene_client = NULL;
    struct scene_client_t *pclient_bak = NULL;
    struct scene_client_t *p_scene_client = NULL;
     /*if client exist, delete old client and add new client */
    list_for_each_entry_safe(temp_scene_client, pclient_bak, &gSceneClients, node){
        if(0 == strcmp(temp_scene_client->client_name, get_scene_client.client_name)){
            list_del(&temp_scene_client->node);
            p_scene_client = temp_scene_client;
            goto UPDATE_PARAMS;
        }
    }
    /***new client to head**/
    if (p_scene_client == NULL) {
        p_scene_client = (struct scene_client_t *)kzalloc(sizeof(struct scene_client_t), GFP_KERNEL);
    }
UPDATE_PARAMS:
    scene_client_copy(p_scene_client, &get_scene_client);

    list_add(&p_scene_client->node, &gSceneClients);

    /*scene action*/
    if(get_device_updating_flag()){
        LOG_ERR("fw updating exit\n");
        return -1;
    }
    cancel_delayed_work(&g_cs_press.delay_run_worker);
    schedule_delayed_work(&g_cs_press.delay_run_worker, msecs_to_jiffies(DEFAULT_RUN_DELAY_TIME));
    return 0;
}


/***********************************
 * client name|scene name|state|l_level|r_level
 * echo "com.android.settings|normal|0|1|2" > 
 * 
***********************************/
static ssize_t cs_proc_scene_switch_write(struct file *file, const char __user *buf,
                                        size_t count, loff_t *offset)
{
    char *kbuf = NULL;

    struct scene_client_t get_scene_client;

    kbuf = kzalloc(count, GFP_KERNEL);
    if (!kbuf) {
        goto exit_flag;
    }
    if (copy_from_user(kbuf, buf, count)) {
        goto exit_kfree;
    }

    if(parse_sence_str(kbuf, &get_scene_client) != 0){
        LOG_DEBUG("parse str err\n");
        goto exit_kfree;
    }

    if(is_scene_client_invalid(get_scene_client) != 0){
       goto exit_kfree;
    }

    cs_insert_scene_client(get_scene_client);

exit_kfree:
    kfree(kbuf);
exit_flag:
    return count;
}

static ssize_t cs_proc_charge_state_write(struct file *file, const char __user *buf,
                                            size_t count, loff_t *offset)
{
    const char *startpos = NULL;
    int tempdata = 0;
    char *kbuf = NULL;
    int err = 0;

    if((count <= 0)||(count > 4)){
        LOG_ERR("argument err\n");
        goto exit_flag;
    }
    kbuf = kzalloc(count, GFP_KERNEL );
    if (!kbuf) {
        goto exit_kfree;
    }
    if (copy_from_user(kbuf, buf, count)) {
        goto exit_kfree;
    }

    startpos = kbuf;

    err = kstrtouint(startpos, 16, &tempdata);
    if (err) {
        LOG_ERR("err kstrtouint\n");
        goto exit_kfree;
    }
    if(tempdata == 0){
        cs_write_charge_state(0);
        LOG_DEBUG("clear charge flag\n");
    }else if(tempdata == 1){
        cs_write_charge_state(1);
        LOG_DEBUG("set charge flag\n");
    }
exit_kfree:
    kfree(kbuf);
exit_flag:
    return count;
}

static ssize_t cs_proc_camera_key_mode_write(struct file *file, const char __user *buf,
                                            size_t count, loff_t *offset)
{
    int tempdata = 0;
    char *kbuf = NULL;

    if (!buf) {
        LOG_ERR("buff is null!\n");
        goto exit_flag;
    }
    if((count <= 0)||(count > 4)){
        LOG_ERR("argument err\n");
        goto exit_flag;
    }
    kbuf = kzalloc(count + 1, GFP_KERNEL);
    if (!kbuf) {
        goto exit_kfree;
    }

    if (copy_from_user(kbuf, buf, count)) {
        goto exit_kfree;
    }

    kbuf[count] = '\0';
    LOG_INFO("%s: kbuf=%s, count=%zu\n", __func__, kbuf, count);
    if (!sscanf(kbuf, "%d", &tempdata)) {
        LOG_ERR("%s: sscanf error.\n", __func__);
        goto exit_kfree;
    }

    mutex_lock(&press_lock);

    LOG_INFO("%s: %d --> %d\n", __func__, g_cs_press.camera_key_mode, tempdata);
    g_cs_press.camera_key_mode = tempdata;
    /*if (g_cs_press.is_long_tap_down) {
        g_cs_press.mode_switch_waiting_up = true;
    } else {*/
    if (g_cs_press.is_suspended) {
        cs_press_set_mode(CAMERA_KEY_SLEEP_MODE);
    } else {
        cs_press_set_mode(g_cs_press.camera_key_mode);
    }

    mutex_unlock(&press_lock);
    /*}*/
exit_kfree:
    kfree(kbuf);
exit_flag:
    return count;
}


static ssize_t cs_proc_camera_key_config_write(struct file *file, const char __user *buf,
                                            size_t count, loff_t *offset)
{
    char *kbuf = NULL;
    unsigned int value[STRENGTH_CFG_NUM] = { 0 };
    int ret = 0;

    if(count <= 0){
        LOG_ERR("argument err\n");
        goto exit_flag;
    }
    kbuf = kzalloc(count + 1, GFP_KERNEL );
    if (!kbuf) {
        goto exit_flag;
    }
    if (copy_from_user(kbuf, buf, count)) {
        goto exit_kfree;
    }
    kbuf[count] = '\0';

    mutex_lock(&press_lock);
    ret = sscanf(kbuf, "%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u",
            &value[0], &value[1], &value[2], &value[3], &value[4], &value[5],
            &value[6], &value[7], &value[8], &value[9], &value[10], &value[11]);

    if (ret == STRENGTH_CFG_NUM) {
        LOG_INFO("%s: value is [%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u].\n", __func__,
                value[0], value[1], value[2], value[3], value[4], value[5],
                value[6], value[7], value[8], value[9], value[10], value[11]);

        g_cs_press.strength_cfg.light_tap_down_strength = value[0] / STRENGTH_PER_LEVEL;
        g_cs_press.strength_cfg.light_tap_up_strength = value[1] / STRENGTH_PER_LEVEL;
        g_cs_press.strength_cfg.heavy_tap_down_strength = value[2] / STRENGTH_PER_LEVEL;
        g_cs_press.strength_cfg.heavy_tap_up_strength = value[3] / STRENGTH_PER_LEVEL;
        g_cs_press.strength_cfg.light_swipe_down_strength = value[4] / STRENGTH_PER_LEVEL;
        g_cs_press.strength_cfg.light_swipe_up_strength = value[5] / STRENGTH_PER_LEVEL;
        g_cs_press.strength_cfg.heavy_swipe_down_strength = value[6] / STRENGTH_PER_LEVEL;
        g_cs_press.strength_cfg.heavy_swipe_up_strength = value[7] / STRENGTH_PER_LEVEL;
        g_cs_press.strength_cfg.area_1 = value[8];
        g_cs_press.strength_cfg.area_2 = value[9];
        g_cs_press.strength_cfg.long_tap_judge_time = value[10] / TIME_MS_PER_LEVEL;
        g_cs_press.strength_cfg.muti_tap_judge_time = value[11] / TIME_MS_PER_LEVEL;

        cs_press_set_trigger_strength(g_cs_press.strength_cfg);
    } else if (ret && (value[0] < CAMERA_KEY_CFG_MAX)) {
        g_cs_press.cfg_chosen = value[0];
        LOG_INFO("%s: choose config %u.\n",
                        __func__, g_cs_press.cfg_chosen);
        if (g_cs_press.camera_key_mode == CAMERA_KEY_CAMERA_MODE) {
            cs_press_set_trigger_strength(g_cs_press.realtime_cfg[g_cs_press.cfg_chosen]);
        } else {
            cs_press_set_trigger_strength(g_cs_press.delay_cfg[g_cs_press.cfg_chosen]);
        }
    } else {
        LOG_ERR("%s: sscanf error.\n", __func__);
    }
    mutex_unlock(&press_lock);
exit_kfree:
    kfree(kbuf);
exit_flag:
    return count;
}

static ssize_t cs_proc_camera_key_report_write(struct file *file, const char __user *buf,
                                            size_t count, loff_t *offset)
{
    char *kbuf = NULL;
    unsigned int key_code = 0;
    unsigned int key_action = 0;
    unsigned int distance = 0;

    if(count <= 0){
        LOG_ERR("argument err\n");
        goto exit_flag;
    }
    kbuf = kzalloc(count, GFP_KERNEL );
    if (!kbuf) {
        goto exit_flag;
    }
    if (copy_from_user(kbuf, buf, count)) {
        goto exit_kfree;
    }

    if (!sscanf(kbuf, "%u,%u,%u",
                    &key_code, &key_action, &distance)) {
        LOG_ERR("%s: sscanf error.\n", __func__);
        goto exit_kfree;
    }

    if ((key_code < KEY_LIGHT_TAP) || (key_code > KEY_SWIPE_DOWN)) {
        LOG_ERR("%s: invalid keycode %u.\n", __func__, key_code);
        goto exit_kfree;
    }

    mutex_lock(&press_lock);
    LOG_INFO("%s: report [%u, %u, %u].\n",
                    __func__, key_code, key_action, distance);

    if (key_action) {
        input_report_key(cs_input_dev, key_code, 1);
        if ((key_code == KEY_SWIPE_UP) || (key_code == KEY_SWIPE_DOWN)) {
            input_report_abs(cs_input_dev, ABS_DISTANCE, distance);
        }
        input_sync(cs_input_dev);
    }
    if (!(key_action & 0x01)) {
        input_report_key(cs_input_dev, key_code, 0);
        if ((key_code == KEY_SWIPE_UP) || (key_code == KEY_SWIPE_DOWN)) {
            input_report_abs(cs_input_dev, ABS_DISTANCE, 0);
        }
        input_sync(cs_input_dev);
    }
    mutex_unlock(&press_lock);

exit_kfree:
    kfree(kbuf);
exit_flag:
    return count;
}

static ssize_t cs_proc_quick_on_close_write(struct file *file, const char __user *buf,
                                            size_t count, loff_t *offset)
{
    int tempdata = 0;
    char *kbuf = NULL;

    if (!buf) {
        LOG_ERR("buff is null!\n");
        goto exit_flag;
    }
    if((count <= 0)||(count > 4)){
        LOG_ERR("argument err\n");
        goto exit_flag;
    }
    kbuf = kzalloc(count + 1, GFP_KERNEL);
    if (!kbuf) {
        goto exit_kfree;
    }

    if (copy_from_user(kbuf, buf, count)) {
        goto exit_kfree;
    }

    kbuf[count] = '\0';
    LOG_INFO("%s: kbuf=%s, count=%zu\n", __func__, kbuf, count);
    if (!sscanf(kbuf, "%d", &tempdata)) {
        LOG_ERR("%s: sscanf error.\n", __func__);
        goto exit_kfree;
    }

    mutex_lock(&press_lock);

    LOG_INFO("%s: %d --> %d\n", __func__, g_cs_press.quick_on_closed, tempdata);
    g_cs_press.quick_on_closed = !!tempdata;
    if (g_cs_press.is_suspended) {
        cs_press_set_mode(CAMERA_KEY_SLEEP_MODE);
    } else {
        cs_press_set_mode(g_cs_press.camera_key_mode);
    }

    mutex_unlock(&press_lock);
exit_kfree:
    kfree(kbuf);
exit_flag:
    return count;
}
static ssize_t cs_proc_gpio_control_write(struct file *file, const char __user *buf,
                                            size_t count, loff_t *offset)
{
    char *kbuf = NULL;
    unsigned int en = 0;
    int ret = 0;

    if(count <= 0){
        LOG_ERR("argument err\n");
        goto exit_flag;
    }
    kbuf = kzalloc(count + 1, GFP_KERNEL );
    if (!kbuf) {
        goto exit_flag;
    }
    if (copy_from_user(kbuf, buf, count)) {
        goto exit_kfree;
    }

    if (sscanf(kbuf, "hard_reset %u", &en)) {
        if (en) {
            cs_press_rstpin_high();
            LOG_INFO("%s: hard_reset gpio set high\n", __func__);
        } else {
            cs_press_rstpin_low();
            LOG_INFO("%s: hard_reset gpio set low\n", __func__);
        }
    } else if (strstr(kbuf, "soft_reset")) {
        cs_press_soft_reset_device();
    } else if (strstr(kbuf, "wdt_reset")) {
        cs_press_wdt_reset_device();
    } else if (sscanf(kbuf, "irq %u", &en)) {
        if (en) {
            cs_irq_enable();
            LOG_INFO("%s: irq enable\n", __func__);
        } else {
            cs_irq_disable();
            LOG_INFO("%s: irq disable\n", __func__);
        }
    } else if (sscanf(kbuf, "power %u", &en)) {
        if (en) {
            if (g_cs_press.vdd_2v8 != NULL) {
                ret = regulator_enable(g_cs_press.vdd_2v8);
                if (!ret) {
                    LOG_INFO("%s: vdd_2v8 enable success\n", __func__);
                    goto exit_kfree;
                }
            }
            LOG_ERR("%s: vdd_2v8 enable fail, ret=%d\n", __func__, ret);
        } else {
            if (g_cs_press.vdd_2v8 != NULL) {
                ret = regulator_disable(g_cs_press.vdd_2v8);
                if (!ret) {
                    LOG_INFO("%s: vdd_2v8 disable success\n", __func__);
                    goto exit_kfree;
                }
            }
            LOG_ERR("%s: vdd_2v8 diable fail, ret=%d\n", __func__, ret);
        }
    }
exit_kfree:
    kfree(kbuf);
exit_flag:
    return count;
}

static ssize_t cs_proc_health_monitor_write(struct file *file, const char __user *buf,
                                            size_t count, loff_t *offset)
{
    int i, j, tempdata = 0;
    char *kbuf = NULL;

    if (!buf) {
        LOG_ERR("buff is null!\n");
        goto exit_flag;
    }
    if((count <= 0)||(count > 4)){
        LOG_ERR("argument err\n");
        goto exit_flag;
    }
    kbuf = kzalloc(count + 1, GFP_KERNEL);
    if (!kbuf) {
        goto exit_kfree;
    }

    if (copy_from_user(kbuf, buf, count)) {
        goto exit_kfree;
    }

    kbuf[count] = '\0';
    LOG_INFO("%s: kbuf=%s, count=%zu\n", __func__, kbuf, count);
    if (!sscanf(kbuf, "%d", &tempdata)) {
        LOG_ERR("%s: sscanf error.\n", __func__);
        goto exit_kfree;
    }

    if (!tempdata) {
        g_cs_press.hard_reset_cnt = 0;
        g_cs_press.soft_reset_cnt = 0;
        g_cs_press.wdt_reset_cnt = 0;
        g_cs_press.tap_force_min = 0;
        g_cs_press.tap_force_max = 0;
        g_cs_press.swipe_force_min = 0;
        g_cs_press.swipe_force_max = 0;
        g_cs_press.double_tap_cnt = 0;
        g_cs_press.bus_error_cnt = 0;
        g_cs_press.fw_update_error = 0;
        for (i = 0; i < CH_COUNT; i++) {
            for (j = 0; j < LAG_MIN_COUNT; j++) {
                g_cs_press.heavy_tap_lag[i][j] = 0;
            }
        }
    }
exit_kfree:
    kfree(kbuf);
exit_flag:
    return count;
}

static int cs_read_left_press_gear_show(struct seq_file *m, void *v)
{
    char ret = 0;
    int i;
    int force_trig;
    unsigned char read_temp[FW_ONE_BLOCK_LENGTH_R] = {0};
    unsigned char check_sum = 0;

    cs_press_wakeup_iic();
    ret = cs_press_iic_read(AP_RW_BUTTON_FORCE_THR_REG, read_temp, CS_FORCE_TRIG_LENGTH);  /* read force/release trig */
    for(i = 0; i < CS_FORCE_TRIG_LENGTH; i++)
    {
        check_sum += read_temp[i];
    }
    if((ret == 0)&&(check_sum == 0))
    {
        force_trig = ((unsigned short)read_temp[1] << 8) | read_temp[0];
        if(force_trig == game_geat[0])
        {
            seq_printf(m,"%d\n", PRESS_GEAR_LEVEL_LOW);
        }else if(force_trig == game_geat[1]){
            seq_printf(m,"%d\n", PRESS_GEAR_LEVEL_MID);
        }else if(force_trig == game_geat[2]){
            seq_printf(m,"%d\n", PRESS_GEAR_LEVEL_HIGH);
        }else{
            seq_printf(m,"err L trig %d\n",force_trig);
        }
    }else{
        seq_printf(m,"err read L trig\n");
        ret = -1;
    }
    return ret;
}

static int cs_read_right_press_gear_show(struct seq_file *m, void *v)
{
    char ret = 0;
    int i;
    int force_trig;
    unsigned char read_temp[FW_ONE_BLOCK_LENGTH_R] = {0};
    unsigned char check_sum = 0;
    cs_press_wakeup_iic();
    ret = cs_press_iic_read(AP_RW_BUTTON_FORCE_THR_REG, read_temp, CS_FORCE_TRIG_LENGTH);  /* read force/release trig */
    for(i = 0; i < CS_FORCE_TRIG_LENGTH; i++)
    {
        check_sum += read_temp[i];
    }
    if((ret == 0)&&(check_sum == 0))
    {
        force_trig = ((unsigned short)read_temp[3] << 8) | read_temp[2];
        if(force_trig == game_geat[0])
        {
            seq_printf(m,"%d\n", PRESS_GEAR_LEVEL_LOW);
        }else if(force_trig == game_geat[1]){
            seq_printf(m,"%d\n", PRESS_GEAR_LEVEL_MID);
        }else if(force_trig == game_geat[2]){
            seq_printf(m,"%d\n", PRESS_GEAR_LEVEL_HIGH);
        }else{
            seq_printf(m,"err: R trig %d\n",force_trig);
        }
    }else{
        seq_printf(m,"err:read R trig\n");
        ret = -1;
    }
    return ret;
}

static int cs_read_shell_temp_show(struct seq_file *m, void *v)
{
    char ret = 0;
    int i;
    int shell_temp = 250;
    unsigned char read_temp[FW_ONE_BLOCK_LENGTH_R] = {0};
    unsigned char check_sum = 0;
    cs_press_wakeup_iic();
    ret = cs_press_iic_read(AP_RW_SHEll_TEMP_REG, read_temp, CS_SHELL_TEMP_LENGTH);
    for(i = 0; i < CS_SHELL_TEMP_LENGTH; i++)
    {
        check_sum += read_temp[i];
    }
    if((ret == 0)&&(check_sum == 0))
    {
        shell_temp = ((unsigned short)read_temp[1] << 8) | read_temp[0];
        seq_printf(m,"%d\n", shell_temp);/*F71 save temp*/
    }else{
        seq_printf(m,"read err\n");
        ret = -1;
    }
    if(cs_press_get_shell_temp(&shell_temp) >= 0)
    {
        seq_printf(m,"%d\n", shell_temp);/*phone temp*/
    }else{
        seq_printf(m,"read phone temp err\n");
    }
    return ret;
}

static int cs_read_power_gpio_show(struct seq_file *m, void *v)
{
    char ret = 0;
    int power_gpio_val;
    if(gpio_is_valid(g_cs_press.power_gpio))
    {
        power_gpio_val = gpio_get_value(g_cs_press.power_gpio);
        seq_printf(m,"%d\n", power_gpio_val);
    }else{
        LOG_ERR("gpio power is invalid\n");
        seq_printf(m,"gpio invalid\n");
        ret = -1;
    }
    return ret;
}

static int cs_read_game_switch_show(struct seq_file *m, void *v)
{
    char ret = 0;
    int i;
    unsigned char read_temp[FW_ONE_BLOCK_LENGTH_R] = {0};
    unsigned char check_sum = 0;
    cs_press_wakeup_iic();
    ret = cs_press_iic_read(AP_RW_WORK_MODE_REG, read_temp, CS_WORK_MODE_LENGTH);
    for(i = 0; i < CS_WORK_MODE_LENGTH; i++)
    {
        check_sum += read_temp[i];
    }
    if((ret == 0)&&(check_sum == 0))
    {
        switch(read_temp[0])
        {
            case 0:
                seq_printf(m,"FTM\n");
                break;
            case 1:
                seq_printf(m,"sleep mode\n");
                break;
            case 2:
                seq_printf(m,"normal mode\n");
                break;
            case 3:
                seq_printf(m,"game mode\n");
                break;
            default:
                seq_printf(m,"read mode err\n");
                break;
        }
    }else{
        seq_printf(m,"read game mode err\n");
        ret = -1;
    }
    return ret;
}

static int cs_read_shell_temp_en_show(struct seq_file *m, void *v)
{
    char ret = 0;
    if(g_cs_press.cs_shell_themal_enable == 0)
    {
        seq_printf(m,"phone temp disable\n");
    }
    if(g_cs_press.cs_shell_themal_enable == 1)
    {
        seq_printf(m,"phone temp enable\n");
    }
    return ret;
}

static int cs_read_ndt_debug_data_show(struct seq_file *m, void *v)
{
    signed short int data_buf[256];
    unsigned char data_len = 0;
    unsigned char i;
    char ret = 0;

    ret = cs_press_read_ndt_debug_data(data_buf, &data_len);
    if(ret == 0)
    {
        for(i = 0; i< data_len; i++)
        {
            seq_printf(m," %d", data_buf[i]);
        }
        seq_printf(m,"\n");
    }
    return ret;
}

static int cs_proc_scene_switch_show(struct seq_file *m, void *v)
{
    char ret = 0;
    struct scene_client_t *temp_scene_client;
    struct scene_client_t *pclient_bak;

    list_for_each_entry_safe(temp_scene_client, pclient_bak, &gSceneClients, node)
    {
        seq_printf(m,"%s|%s|%d|%d|%d\n",temp_scene_client->client_name,temp_scene_client->pscene->scene_name,\
                    temp_scene_client->state,temp_scene_client->left_geat_level,temp_scene_client->right_geat_level);
    }
    return ret;
}

static int cs_proc_charge_state_show(struct seq_file *m, void *v)
{
    char ret = 0;
    seq_printf(m,"%d\n",cs_read_charge_state());
    return ret;
}

static int cs_proc_camera_key_mode_show(struct seq_file *m, void *v)
{
    char ret = 0;
    seq_printf(m, "%d", g_cs_press.camera_key_mode);
    return ret;
}

static int cs_proc_camera_key_config_show(struct seq_file *m, void *v)
{
    char ret = 0;
    trigger_strength_config strength_cfg;

#ifdef CAMERA_KEY
    camera_key_config_read(&strength_cfg);
    seq_printf(m, "tap_strength[%u,%u,%u,%u], swipe_strength[%u,%u,%u,%u], area[%u,%u], judge_time[%u,%u]\n",
        strength_cfg.light_tap_down_strength * STRENGTH_PER_LEVEL,
        strength_cfg.light_tap_up_strength * STRENGTH_PER_LEVEL,
        strength_cfg.heavy_tap_down_strength * STRENGTH_PER_LEVEL,
        strength_cfg.heavy_tap_up_strength * STRENGTH_PER_LEVEL,
        strength_cfg.light_swipe_down_strength * STRENGTH_PER_LEVEL,
        strength_cfg.light_swipe_up_strength * STRENGTH_PER_LEVEL,
        strength_cfg.heavy_swipe_down_strength * STRENGTH_PER_LEVEL,
        strength_cfg.heavy_swipe_up_strength * STRENGTH_PER_LEVEL,
        strength_cfg.area_1, strength_cfg.area_2,
        strength_cfg.long_tap_judge_time * TIME_MS_PER_LEVEL,
        strength_cfg.muti_tap_judge_time * TIME_MS_PER_LEVEL);
#endif
    return ret;
}

static int cs_proc_quick_on_close_show(struct seq_file *m, void *v)
{
    char ret = 0;
    seq_printf(m, "%d", g_cs_press.quick_on_closed);
    return ret;
}

static int cs_proc_health_monitor_show(struct seq_file *m, void *v)
{
    int i, cnt, ret = 0;
    unsigned char read_temp[FW_ONE_BLOCK_LENGTH_R] = {0};

    seq_printf(m,"ic:CSA37F71\n");
    cs_press_wakeup_iic();
    ret = cs_press_iic_read(AP_VERSION_REG, read_temp, CS_FW_VERSION_LENGTH);  /*FW Version*/
    if(ret == 0){
        seq_printf(m,"fw_ver:%d %d %d %d\n",
             read_temp[0], read_temp[1], read_temp[2], read_temp[3]);
    } else {
        seq_printf(m,"fw_ver:read error\n");
    }
    ret = cs_press_get_offset(g_cs_press.dac_offset);
    if (ret == 0) {
        seq_printf(m, "ch1_offset:%d\n", g_cs_press.dac_offset[0]);
        seq_printf(m, "ch2_offset:%d\n", g_cs_press.dac_offset[1]);
    } else {
        seq_printf(m, "ch1_offset:read error\n");
        seq_printf(m, "ch2_offset:read error\n");
    }
    ret = cs_press_get_noise_var(g_cs_press.dac_noise_var);
    if (ret == 0) {
        seq_printf(m, "ch1_noise_var:%d\n", g_cs_press.dac_noise_var[0]);
        seq_printf(m, "ch2_noise_var:%d\n", g_cs_press.dac_noise_var[1]);
    } else {
        seq_printf(m, "ch1_noise_var:read error\n");
        seq_printf(m, "ch2_noise_var:read error\n");
    }
    seq_printf(m, "ch1_offset_boot:%d\n", g_cs_press.dac_offset_boot[0]);
    seq_printf(m, "ch2_offset_boot:%d\n", g_cs_press.dac_offset_boot[1]);
    seq_printf(m, "ch1_noise_var_boot:%d\n", g_cs_press.dac_noise_var_boot[0]);
    seq_printf(m, "ch2_noise_var_boot:%d\n", g_cs_press.dac_noise_var_boot[1]);
    seq_printf(m, "tap_force_min:%d\n", g_cs_press.tap_force_min);
    seq_printf(m, "tap_force_max:%d\n", g_cs_press.tap_force_max);
    seq_printf(m, "swipe_force_min:%d\n", g_cs_press.swipe_force_min);
    seq_printf(m, "swipe_force_max:%d\n", g_cs_press.swipe_force_max);
    seq_printf(m, "double_tap_cnt:%d\n", g_cs_press.double_tap_cnt);
    for (i = 0; i < CH_COUNT; i++) {
        seq_printf(m, "ch%d_lag:%d,%d,%d,%d,%d\n", i + 1,
                    g_cs_press.heavy_tap_lag[i][0], g_cs_press.heavy_tap_lag[i][1], g_cs_press.heavy_tap_lag[i][2],
                    g_cs_press.heavy_tap_lag[i][3], g_cs_press.heavy_tap_lag[i][4]);
    }
    if (g_cs_press.hard_reset_cnt)
        seq_printf(m, "hard_reset_cnt:%d\n", g_cs_press.hard_reset_cnt);
    if (g_cs_press.soft_reset_cnt)
        seq_printf(m, "soft_reset_cnt:%d\n", g_cs_press.soft_reset_cnt);
    if (g_cs_press.wdt_reset_cnt)
        seq_printf(m, "wdt_reset_cnt:%d\n", g_cs_press.wdt_reset_cnt);
    if (g_cs_press.fw_update_error)
        seq_printf(m, "fw_update_error:0x%x\n", g_cs_press.fw_update_error);
    if (g_cs_press.bus_error_cnt) {
        seq_printf(m, "bus_error_cnt:%d\n", g_cs_press.bus_error_cnt);
        cnt = (g_cs_press.bus_error_cnt > BUS_ERROR_MSG_CNT) ? BUS_ERROR_MSG_CNT : g_cs_press.bus_error_cnt;
        for (i = 0; i < cnt; i++) {
            seq_printf(m, "bus_error_msg[%d]:%s\n", i, g_cs_press.bus_error_msg[i]);
        }
    }

    return 0;
}

static int cs_proc_read_boot_version_open(struct inode *inode, struct file *filp)
{
    return single_open(filp,cs_read_boot_version_show,NULL);
}

static int cs_proc_auto_test_open(struct inode *inode, struct file *filp)
{
    return single_open(filp,cs_auto_test_show,NULL);
}

static int cs_proc_read_left_press_gear_open(struct inode *inode, struct file *filp)
{
    return single_open(filp, cs_read_left_press_gear_show, NULL);
}

static int cs_proc_read_right_press_gear_open(struct inode *inode, struct file *filp)
{
    return single_open(filp, cs_read_right_press_gear_show, NULL);
}

static int cs_proc_read_shell_temp_open(struct inode *inode, struct file *filp)
{
    return single_open(filp, cs_read_shell_temp_show, NULL);
}

static int cs_proc_read_power_gpio_open(struct inode *inode, struct file *filp)
{
    return single_open(filp, cs_read_power_gpio_show, NULL);
}

static int cs_proc_read_game_switch_open(struct inode *inode, struct file *filp)
{
    return single_open(filp, cs_read_game_switch_show, NULL);
}

static int cs_proc_read_shell_temp_en_open(struct inode *inode, struct file *filp)
{
    return single_open(filp, cs_read_shell_temp_en_show, NULL);
}

static int cs_proc_read_ndt_debug_data_open(struct inode *inode, struct file *filp)
{
    return single_open(filp, cs_read_ndt_debug_data_show, NULL);
}

static int cs_proc_scene_switch_open(struct inode *inode, struct file *filp)
{
    return single_open(filp, cs_proc_scene_switch_show, NULL);
}

static int cs_proc_charge_state_open(struct inode *inode, struct file *filp)
{
    return single_open(filp, cs_proc_charge_state_show, NULL);
}

static int cs_proc_camera_key_mode_open(struct inode *inode, struct file *filp)
{
    return single_open(filp, cs_proc_camera_key_mode_show, pde_data(inode));
}

static int cs_proc_camera_key_config_open(struct inode *inode, struct file *filp)
{
    return single_open(filp, cs_proc_camera_key_config_show, pde_data(inode));
}

static int cs_proc_quick_on_close_open(struct inode *inode, struct file *filp)
{
    return single_open(filp, cs_proc_quick_on_close_show, pde_data(inode));
}
static int cs_proc_health_monitor_open(struct inode *inode, struct file *filp)
{
    return single_open(filp, cs_proc_health_monitor_show, pde_data(inode));
}

static int cs_proc_show(struct seq_file *m,void *v)
{
    return 0;
}

static int cs_proc_open(struct inode *inode, struct file *filp)
{
    return single_open(filp,cs_proc_show,NULL);
}

static ssize_t cs_proc_write(struct file *file, const char __user *buf,
                             size_t count, loff_t *offset)
{
    char *kbuf = NULL;

    kbuf = kzalloc(count, GFP_KERNEL );
    if (!kbuf) {
        goto exit;
    }
    if (copy_from_user(kbuf, buf, count)) {
        goto exit_kfree;
    }
exit_kfree:
    kfree(kbuf);
exit:
    return count;
}

/**
  * @brief    dts parse
  * @param
  * @retval
*/
int cs_parse_dts(struct i2c_client *pdev)
{
    int ret = -1;
#ifdef INT_SET_EN

    cs_press_irq_gpio = of_get_named_gpio((pdev->dev).of_node,"irq-gpio",0);
    if(!gpio_is_valid(cs_press_irq_gpio))
    {
        dev_err(&pdev->dev, "cs_press request_irq IRQ fail");
    }
    else
    {
        ret = gpio_request(cs_press_irq_gpio, "irq-gpio");
        if(ret)
        {
            dev_err(&pdev->dev, "cspress request_irq IRQ fail !,ret=%d.\n", ret);
        }
        ret = gpio_direction_input(cs_press_irq_gpio);
        msleep(50);
        cs_press_irq_num = gpio_to_irq(cs_press_irq_gpio);
        ret = request_irq(cs_press_irq_num,
          (irq_handler_t)cs_press_interrupt_handler,
          IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
          "CS_PRESS-eint", g_cs_press.device_irq);
        if (ret > 0) {
            ret = -1;
            dev_err(&pdev->dev, "cs_press request_irq IRQ fail !,ret=%d.\n", ret);
        }
    }
#endif
    g_cs_press.rst_gpio = of_get_named_gpio((pdev->dev).of_node, "reset-gpio", 0);
    if(!gpio_is_valid(g_cs_press.rst_gpio))
    {
        dev_err(&pdev->dev, "cs_press request_rst fail");
    } else {
        ret = gpio_request(g_cs_press.rst_gpio, "reset-gpio");
            if(ret)
            {
                dev_err(&pdev->dev, "cs_press request rst fail !,ret=%d.\n", ret);
            }
            ret = gpio_direction_output(g_cs_press.rst_gpio,0);
            dev_err(&pdev->dev, "gpio_direction_output set 0:rst_gpio=%d, ret=%d.\n", g_cs_press.rst_gpio, ret);
            msleep(50);
        gpio_set_value(g_cs_press.rst_gpio, 0);
    }

    g_cs_press.power_gpio = of_get_named_gpio((pdev->dev).of_node, "enable1v8_gpio", 0);
    if(!gpio_is_valid(g_cs_press.power_gpio))
    {
        dev_err(&pdev->dev, "cs_press enable1v8_gpio fail");
    } else {
        ret = gpio_request(g_cs_press.power_gpio, "enable1v8_gpio");
            if(ret)
            {
                dev_err(&pdev->dev, "cs_press request enable1v8_gpio fail !,ret=%d.\n", ret);
            }
            ret = gpio_direction_output(g_cs_press.power_gpio,0);
            msleep(50);
        gpio_set_value(g_cs_press.power_gpio, 1);
    }

    g_cs_press.vdd_2v8 = regulator_get(&pdev->dev, "vdd_2v8");
    if (g_cs_press.vdd_2v8 != NULL) {
        dev_err(&pdev->dev,"%s:vdd_2v8 is not NULL\n", __func__);
        ret = regulator_enable(g_cs_press.vdd_2v8);
        if (ret)
            dev_err(&pdev->dev,"%s: vdd_2v8 enable fail\n", __func__);
    } else {
            dev_err(&pdev->dev,"%s: vdd_2v8 is NULL\n", __func__);
    }
    cs_press_para_init();
    LOG_ERR("end---\n");
    return 0;
}
/**
  * @brief    free resource from dts info
  * @param
  * @retval 0:success, -1: fail
  */
void cs_unregister_dts(void){
    /*GPIO unregister*/
    if(gpio_is_valid(g_cs_press.rst_gpio)){
        gpio_free(g_cs_press.rst_gpio);
        LOG_DEBUG("reset gpio free\n");
    }
    #ifdef INT_SET_EN
    if(gpio_is_valid(cs_press_irq_gpio)){
        free_irq(cs_press_irq_num,g_cs_press.device_irq);
        gpio_free(cs_press_irq_gpio);
        LOG_DEBUG("irq gpio free\n");
        g_cs_press.device_irq = NULL;
    }
    #endif
}

static const char proc_list[PROC_FOPS_NUM][PROC_NAME_LEN]={
    "fw_info",
    "rw_reg",
    "cs_iic_test",
    "read_rawdata",
    "left_press_level",
    "right_press_level",
    "read_cali_coef",
    "read_force_data",
    "fw_update_file",
    "read_boot_version",
    "auto_test",
    "left_press_gear",
    "right_press_gear",
    "shell_temp",
    "power_manage",
    "press_game_switch",
    "shell_temp_en",
    "ndt_debug",
    "gpio_toggle",
    "MechanicalKeyParams",
    "press_charge_state",
    "camera_key_mode",
    "camera_key_config",
    "camera_key_report",
    "quick_on_close",
    "gpio_control",
    "local_fw_info",
    "health_info",
};

#ifndef ALIENTEK
static const struct proc_ops proc_fops[PROC_FOPS_NUM] = {
#else
static const struct file_operations proc_fops[PROC_FOPS_NUM] = {
#endif

    FOPS_ARRAY(cs_proc_fw_info_open, NULL),    /*fw_info*/
    FOPS_ARRAY(cs_proc_rw_reg_open, cs_proc_rw_reg_write),  /*rw_reg*/
    FOPS_ARRAY(cs_proc_open, cs_proc_iic_rw_test_write),   /*IIC read write test*/
    FOPS_ARRAY(cs_proc_get_rawdata_open, NULL),
    FOPS_ARRAY(cs_proc_read_left_press_level_open, cs_proc_left_press_level_write),
    FOPS_ARRAY(cs_proc_read_right_press_level_open, cs_proc_right_press_level_write),
    FOPS_ARRAY(cs_proc_calicoef_open, NULL),
    FOPS_ARRAY(cs_proc_get_forcedata_open, NULL),
    FOPS_ARRAY(cs_proc_open, cs_proc_fw_file_update_write),
    FOPS_ARRAY(cs_proc_read_boot_version_open, cs_proc_write),
    FOPS_ARRAY(cs_proc_auto_test_open, cs_proc_write),

    FOPS_ARRAY(cs_proc_read_left_press_gear_open, cs_proc_left_press_gear_write),
    FOPS_ARRAY(cs_proc_read_right_press_gear_open, cs_proc_right_press_gear_write),
    FOPS_ARRAY(cs_proc_read_shell_temp_open, cs_proc_shell_temp_write),
    FOPS_ARRAY(cs_proc_read_power_gpio_open, cs_proc_power_gpio_write),
    FOPS_ARRAY(cs_proc_read_game_switch_open, cs_proc_game_switch_write),
    FOPS_ARRAY(cs_proc_read_shell_temp_en_open, cs_proc_shell_temp_en_write),
    FOPS_ARRAY(cs_proc_read_ndt_debug_data_open, cs_proc_ndt_debug_int_write),
    FOPS_ARRAY(cs_proc_open, cs_proc_gpio_toggle_write),
    FOPS_ARRAY(cs_proc_scene_switch_open, cs_proc_scene_switch_write),
    FOPS_ARRAY(cs_proc_charge_state_open, cs_proc_charge_state_write),
    FOPS_ARRAY(cs_proc_camera_key_mode_open, cs_proc_camera_key_mode_write),
    FOPS_ARRAY(cs_proc_camera_key_config_open, cs_proc_camera_key_config_write),
    FOPS_ARRAY(cs_proc_open, cs_proc_camera_key_report_write),
    FOPS_ARRAY(cs_proc_quick_on_close_open, cs_proc_quick_on_close_write),
    FOPS_ARRAY(cs_proc_open, cs_proc_gpio_control_write),
    FOPS_ARRAY(cs_proc_local_fw_info_open, NULL),    /*local_fw_info*/
    FOPS_ARRAY(cs_proc_health_monitor_open, cs_proc_health_monitor_write),
};
/**
  * @brief  cs_sys_create
  * @param
  * @retval
*/
static int cs_procfs_create(void)
{
    int ret = 0;
    int i = 0;
    struct proc_dir_entry *file;

    g_cs_press.p_proc_dir = proc_mkdir(CS_PRESS_NAME, NULL);
    if(g_cs_press.p_proc_dir == NULL){
        ret = -1;
        goto exit;
    }
    for(i = 0; i < PROC_FOPS_NUM; i++)
    {
        file = proc_create_data(proc_list[i], 0666, g_cs_press.p_proc_dir, &proc_fops[i], NULL);
        /*file = proc_create(proc_list[i],0644,NULL,&proc_fops[i]);*/
        if(file == NULL){
            ret = -1;
            LOG_ERR("proc %s create failed",proc_list[i]);
            goto err_flag;
        }
    }
    return 0;
err_flag:
    remove_proc_entry(CS_PRESS_NAME,NULL);
exit:
    return ret;
}

static void cs_procfs_delete(void)
{
    int i = 0;

    for(i = 0;i < PROC_FOPS_NUM;i++)
    {
        remove_proc_entry(proc_list[i], g_cs_press.p_proc_dir);
        LOG_DEBUG("proc %s is removed", proc_list[i]);
    }
    remove_proc_entry(CS_PRESS_NAME,NULL);
}

#if IS_ENABLED(CONFIG_QCOM_PANEL_EVENT_NOTIFIER)
static void cs_press_panel_notifier_callback(enum panel_event_notifier_tag tag,
         struct panel_event_notification *notification, void *client_data)
{
    if (!notification) {
        LOG_ERR("Invalid notification\n");
        return;
    }
    if (notification->notif_type < DRM_PANEL_EVENT_FOR_TOUCH) {
        LOG_INFO("Notification type:%d, early_trigger:%d",
            notification->notif_type,
            notification->notif_data.early_trigger);
    }

    if (g_hall_dev->bus_ready == false) {
        LOG_ERR("bus_ready not ready, tp exit\n");
        return;
    }

    mutex_lock(&press_lock);
    if (notification->notif_type == DRM_PANEL_EVENT_UNBLANK && g_cs_press.is_suspended) {
        cs_press_set_mode(g_cs_press.camera_key_mode);
        g_cs_press.is_suspended = false;
    } else if (notification->notif_type == DRM_PANEL_EVENT_BLANK && !g_cs_press.is_suspended) {
        cs_press_set_mode(CAMERA_KEY_SLEEP_MODE);
        g_cs_press.is_suspended = true;
    }
    mutex_unlock(&press_lock);
}

#elif IS_ENABLED(CONFIG_OPLUS_MTK_DRM_GKI_NOTIFY) || IS_ENABLED(CONFIG_OPLUS_DEVICE_INFO_MTK_PLATFORM)
static int cs_press_mtk_drm_notifier_callback(struct notifier_block *nb,
    unsigned long event, void *data)
{
    int *blank = (int *)data;

    LOG_INFO("mtk gki notifier event:%lu, blank:%d",
            event, *blank);

    mutex_lock(&press_lock);
    if (*blank == MTK_DISP_BLANK_UNBLANK && g_cs_press.is_suspended) {
        cs_press_set_mode(g_cs_press.camera_key_mode);
        g_cs_press.is_suspended = false;
    } else if (*blank == MTK_DISP_BLANK_POWERDOWN && !g_cs_press.is_suspended) {
        cs_press_set_mode(CAMERA_KEY_SLEEP_MODE);
        g_cs_press.is_suspended = true;
    }
    mutex_unlock(&press_lock);

    return 0;
}

#else
static int fb_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{
    int *blank;
#if IS_ENABLED(CONFIG_DRM_MSM) || IS_ENABLED(CONFIG_DRM_OPLUS_NOTIFY)
    struct msm_drm_notifier *evdata = data;
#else
    struct fb_event *evdata = data;
#endif

    /*to aviod some kernel bug (at fbmem.c some local veriable are not initialized)*/
#if IS_ENABLED(CONFIG_DRM_MSM) || IS_ENABLED(CONFIG_DRM_OPLUS_NOTIFY)
    if (event != MSM_DRM_EARLY_EVENT_BLANK && event != MSM_DRM_EVENT_BLANK)
#else
    if (event != FB_EARLY_EVENT_BLANK && event != FB_EVENT_BLANK)
#endif
        return 0;

    if (evdata && evdata->data) {
        blank = evdata->data;
        LOG_INFO("%s: event = %ld, blank = %d\n", __func__, event, *blank);
        mutex_lock(&press_lock);
#if IS_ENABLED(CONFIG_DRM_MSM) || IS_ENABLED(CONFIG_DRM_OPLUS_NOTIFY)
        if (*blank == MSM_DRM_BLANK_UNBLANK && g_cs_press.is_suspended) { /*resume*/
#else
        if (*blank == FB_BLANK_UNBLANK && g_cs_press.is_suspended) {  /*resume*/
#endif
            cs_press_set_mode(g_cs_press.camera_key_mode);
            g_cs_press.is_suspended = false;
#if IS_ENABLED(CONFIG_DRM_MSM) || IS_ENABLED(CONFIG_DRM_OPLUS_NOTIFY)
        } else if (*blank == MSM_DRM_BLANK_BLANK && !g_cs_press.is_suspended) { /*suspend*/
#else
        } else if (*blank == FB_BLANK_BLANK && !g_cs_press.is_suspended) {  /*suspend*/
#endif
            cs_press_set_mode(CAMERA_KEY_SLEEP_MODE);
            g_cs_press.is_suspended = true;
        }
        mutex_unlock(&press_lock);
    }

    return 0;
}
#endif

#if IS_ENABLED(CONFIG_DRM_OPLUS_PANEL_NOTIFY) || IS_ENABLED(CONFIG_QCOM_PANEL_EVENT_NOTIFIER)
struct drm_panel *cs_press_dev_get_panel(struct device_node *of_node)
{
    int i;
    int count;
    struct device_node *node;
    struct drm_panel *panel;
    struct device_node *np;
    char disp_node[32] = {0};

    np = of_find_node_by_name(NULL, "oplus,dsi-display-dev");
    if (!np) {
        LOG_INFO("[oplus,dsi-display-dev] is missing, try to find [panel] node \n");
        np = of_node;
        LOG_INFO("np full name = %s \n", np->full_name);
        strncpy(disp_node, "panel", sizeof("panel"));
    } else {
        LOG_ERR("[oplus,dsi-display-dev] node found \n");
        /* for primary panel */
        strncpy(disp_node, "oplus,dsi-panel-primary", sizeof("oplus,dsi-panel-primary"));
    }
    LOG_INFO("disp_node = %s \n", disp_node);

    count = of_count_phandle_with_args(np, disp_node, NULL);
    if (count <= 0) {
        LOG_ERR("can not find [%s] node in dts \n", disp_node);
        return NULL;
    }

    for (i = 0; i < count; i++) {
        node = of_parse_phandle(np, disp_node, i);
        panel = of_drm_find_panel(node);
        LOG_INFO("panel[%d] IS_ERR =%d \n", i, IS_ERR(panel));
        of_node_put(node);
        if (!IS_ERR(panel)) {
            LOG_ERR("Find available panel\n");
            return panel;
        }
    }

    return NULL;
}
#endif

int cs_press_register_notifier(void)
{
    int ret = 0;
#if IS_ENABLED(CONFIG_QCOM_PANEL_EVENT_NOTIFIER)
    void *cookie = NULL;
#endif
#if IS_ENABLED(CONFIG_DRM_OPLUS_PANEL_NOTIFY)
    g_cs_press.active_panel = g_hall_dev->active_panel;
    g_cs_press.fb_notif.notifier_call = fb_notifier_callback;

    if (g_cs_press.active_panel)
        ret = drm_panel_notifier_register(g_cs_press.active_panel,
                &g_cs_press.fb_notif);
#elif IS_ENABLED(CONFIG_QCOM_PANEL_EVENT_NOTIFIER)
    g_cs_press.active_panel = g_hall_dev->active_panel;
    if (g_cs_press.active_panel) {
        cookie = panel_event_notifier_register(PANEL_EVENT_NOTIFICATION_PRIMARY,
                PANEL_EVENT_NOTIFIER_CLIENT_PRESS, g_cs_press.active_panel,
                &cs_press_panel_notifier_callback, g_cs_press);

        if (!cookie) {
            LOG_ERR("Unable to register fb_notifier: %d\n", ret);
        } else {
            g_cs_press.notifier_cookie = cookie;
        }
    }

#elif IS_ENABLED(CONFIG_OPLUS_MTK_DRM_GKI_NOTIFY) || IS_ENABLED(CONFIG_OPLUS_DEVICE_INFO_MTK_PLATFORM)
    g_cs_press.disp_notifier.notifier_call = cs_press_mtk_drm_notifier_callback;
    if (mtk_disp_notifier_register("Oplus_Press", &g_cs_press.disp_notifier)) {
        LOG_ERR("Failed to register disp notifier client!!\n");
    }

#elif IS_ENABLED(CONFIG_DRM_MSM) || IS_ENABLED(CONFIG_DRM_OPLUS_NOTIFY)
    g_cs_press.fb_notif.notifier_call = fb_notifier_callback;
    ret = msm_drm_register_client(&g_cs_press.fb_notif);

    if (ret) {
        LOG_ERR("Unable to register fb_notifier: %d\n", ret);
    }

#elif IS_ENABLED(CONFIG_FB)
    g_cs_press.fb_notif.notifier_call = fb_notifier_callback;
    ret = fb_register_client(&g_cs_press.fb_notif);

    if (ret) {
        LOG_ERR("Unable to register fb_notifier: %d\n", ret);
    }
#endif/*CONFIG_FB*/
    return ret;
}

int cs_press_unregister_notifier(void)
{
    int ret = 0;
#if IS_ENABLED(CONFIG_DRM_OPLUS_PANEL_NOTIFY)
    if (g_cs_press.active_panel && g_cs_press.fb_notif.notifier_call) {
        ret = drm_panel_notifier_unregister(g_cs_press.active_panel,
            &g_cs_press.fb_notif);
        if (ret) {
            LOG_ERR("Unable to unregister fb_notifier: %d\n", ret);
        }
    }
#elif IS_ENABLED(CONFIG_QCOM_PANEL_EVENT_NOTIFIER)
    if (g_cs_press.active_panel && g_cs_press.notifier_cookie) {
        panel_event_notifier_unregister(g_cs_press.notifier_cookie);
    }
#elif IS_ENABLED(CONFIG_OPLUS_MTK_DRM_GKI_NOTIFY) || IS_ENABLED(CONFIG_OPLUS_DEVICE_INFO_MTK_PLATFORM)
    if (g_cs_press.disp_notifier.notifier_call) {
        mtk_disp_notifier_unregister(&g_cs_press.disp_notifier);
    }
#elif IS_ENABLED(CONFIG_DRM_MSM) || IS_ENABLED(CONFIG_DRM_OPLUS_NOTIFY)
    if (g_cs_press.fb_notif.notifier_call) {
        ret = msm_drm_unregister_client(&g_cs_press.fb_notif);

        if (ret) {
            LOG_ERR("Unable to register fb_notifier: %d\n", ret);
        }
    }
#elif IS_ENABLED(CONFIG_FB)
    if (g_cs_press.fb_notif.notifier_call) {
        ret = fb_unregister_client(&g_cs_press.fb_notif);

        if (ret) {
            LOG_ERR("Unable to unregister fb_notifier: %d\n", ret);
        }
    }
#endif/*CONFIG_FB*/
    return ret;
}

/********misc node for ndt*********/
static int csa37f71_open(struct inode *inode, struct file *filp)
{
    LOG_DEBUG("csa37f71_open\n");
    if(g_cs_press.client == NULL)
    {
        LOG_DEBUG("client is null\n");
        return -1;
    }
    return 0;
}

static ssize_t csa37f71_write(struct file *file, const char __user *buf,
        size_t count, loff_t *offset)
{
    int err;
    char *kbuf = NULL;
    char reg;
     /*****fw updating return****/
    if(get_device_updating_flag()){
        LOG_ERR("updating\n");
        return -2;
    }

    kbuf = kzalloc(count, GFP_KERNEL );
    if (!kbuf)
    {
        LOG_DEBUG("kbuf is null\n");
        err = -ENOMEM;
        goto exit;
    }

    if (copy_from_user(&reg, buf, 1) || copy_from_user(kbuf, buf+1, count))
    {
        LOG_DEBUG("copy from user err\n");
        err = -EFAULT;
        goto exit_kfree;
    }
    err = cs_press_iic_write(reg, kbuf, count);
    if(err >= 0)
    {
        err = 1;
    }

    exit_kfree:
    kfree(kbuf);
    exit:
    return err;
}

static ssize_t csa37f71_read(struct file *filp, char __user *buf, size_t count, loff_t *off)
{
    int err = 0;
    char *kbuf = NULL;
    char reg = 0;
     /*****fw updating return****/
    if(get_device_updating_flag()){
        LOG_ERR("updating\n");
        return -2;
    }

    kbuf = kzalloc(count, GFP_KERNEL );
    if (!kbuf) {
        err = -ENOMEM;
        LOG_DEBUG("kbuf is null\n");
        goto exit;
    }
    /*get reg addr buf[0]*/
    if (copy_from_user(&reg, buf, 1)) {
        err = -EFAULT;
        LOG_DEBUG("copy from user err\n");
        goto exit_kfree;
    }
    err = cs_press_iic_read(reg, kbuf, count);
    if (err < 0){
        goto exit_kfree;
    }
    if (copy_to_user(buf+1, kbuf, count)){
        LOG_DEBUG("copy from user err\n");
        err = -EFAULT;
    }else{
        err = 1;
    }
    exit_kfree:
    kfree(kbuf);
    exit:
    return err;
}

static int csa37f71_release(struct inode *inode, struct file *filp)
{
    return 0;
}
/*
 * @ misc device file operation
 */
static const struct file_operations csa37f71_fops = {
    .owner = THIS_MODULE,
    .open = csa37f71_open,
    .write = csa37f71_write,
    .read = csa37f71_read,
    .release = csa37f71_release,
};

static struct miscdevice csa37f71_misc = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = MISC_DEVICE_NAME,
    .fops  = &csa37f71_fops,
};

static int csa37f71_probe(struct i2c_client *client)
{
     int ret = -1;

    LOG_DEBUG("probe init\n");
    ret = misc_register(&csa37f71_misc); /*dev node*/
    if(ret){
        LOG_DEBUG("misc_register err %d\n",ret);
    }
    g_cs_press.client = client;
    cs_parse_dts(g_cs_press.client);
    cs_press_struct_init();
/* ts check panel dt */
#if IS_ENABLED(CONFIG_DRM_OPLUS_PANEL_NOTIFY) || IS_ENABLED(CONFIG_QCOM_PANEL_EVENT_NOTIFIER)
    /* get spi of_node from spi_register_driver */
    for(retry = 0; retry < 10; retry++) {
        g_cs_press.active_panel = cs_press_dev_get_panel(g_cs_press.client->dev.of_node);
        if (g_cs_press.active_panel) {
            LOG_ERR("Success to get panel info\n");
            break;
        }
        msleep(500);
    }

    if (retry == 10) {
        LOG_ERR("ts check panel dt failed\n");
        misc_deregister(&csa37f71_misc); /*dev node*/
        return -EPROBE_DEFER; /* retry */
    }
#endif
    cs_procfs_create();                 /*proc node*/
#ifdef INT_SET_EN
    eint_init();
#endif
    g_cs_press.camera_key_mode = CAMERA_KEY_DEFAULT_MODE;
    camera_key_config_read(&g_cs_press.strength_cfg);

    g_cs_press.suspend_lock = wakeup_source_register(NULL, "cs_press_wakelock");
    if (!g_cs_press.suspend_lock) {
        pr_err("wakeup source init failed.\n");
    }

    INIT_DELAYED_WORK(&g_cs_press.update_worker, update_work_func);
    schedule_delayed_work(&g_cs_press.update_worker, msecs_to_jiffies(20000));
    LOG_INFO("update_work_func start,delay 20s.\n");

    INIT_DELAYED_WORK(&g_cs_press.delay_run_worker, set_mode_from_client_list);

#ifndef ALIENTEK
    INIT_DELAYED_WORK(&g_cs_press.shell_temp_worker, shell_temp_work_func);
    g_cs_press.cs_thermal_zone_device = thermal_zone_get_zone_by_name("hot-pock-therm");
    if (IS_ERR(g_cs_press.cs_thermal_zone_device))
    {
        g_cs_press.cs_shell_themal_init_flag = 0;
        LOG_ERR("Can't get hot-pock-therm\n");
    }
#endif
    ret = cs_press_register_notifier();
    if (ret < 0) {
        LOG_ERR("cs_press_register_notifier failed\n");
    }
/*
#ifdef I2C_CHECK_SCHEDULE
    INIT_DELAYED_WORK(&g_cs_press.i2c_check_worker, i2c_check_func);
    schedule_delayed_work(&g_cs_press.i2c_check_worker, msecs_to_jiffies(CHECK_DELAY_TIME));
    LOG_ERR("i2c_check_func start,delay 20s.\n");
#endif
*/
    LOG_ERR("end!\n");
    return 0;
}

static int csa37f71_suspend(struct device *dev)
{
    enable_irq_wake(cs_press_irq_num);
    LOG_DEBUG("suspend\n");
    return 0;
}

static int csa37f71_resume(struct device *dev)
{
    disable_irq_wake(cs_press_irq_num);
    LOG_DEBUG("resume\n");
    return 0;
}

static void csa37f71_remove(struct i2c_client *client)
{
    /* delete device */
    if(g_cs_press.client == NULL){
        return;
    }
    LOG_DEBUG("csa37f71_remove\n");
    if (g_cs_press.suspend_lock) {
        wakeup_source_unregister(g_cs_press.suspend_lock);
    }
    cs_press_unregister_notifier();
    cs_unregister_dts();
    cs_procfs_delete();
#ifdef INT_SET_EN
    eint_exit();
#endif
    misc_deregister(&csa37f71_misc);
    g_cs_press.client = NULL;
}

/*
 * @traditional match list,use thie when not using dts
 */
static const struct i2c_device_id csa37f71_id[] = {
    {CS_PRESS_NAME, 0},
    {/*northing to be done*/},
};

/*
 * @dts match list
 */
static const struct of_device_id of_csa37f71_match[] = {
    { .compatible = "chipsea,csa37f71" },
    {/*northing to be done*/},
};

/**
 * @Support fast loading of hot swap devices
 */
MODULE_DEVICE_TABLE(i2c, csa37f71_id);

static const struct dev_pm_ops cs_press_pm_ops = {
    .suspend = csa37f71_suspend,
    .resume = csa37f71_resume,
};

/*i2c driver struct*/
static struct i2c_driver csa37f71_driver = {
    .probe = csa37f71_probe,
    .remove = csa37f71_remove,
    .driver = {
            .owner = THIS_MODULE,
            .name = CS_PRESS_NAME,
            .of_match_table = of_match_ptr(of_csa37f71_match),/*if use dts,use of_match_table to match*/
            .pm = &cs_press_pm_ops,
    },
    .id_table = csa37f71_id,
};

static int __init csa37f71_init(void)
{
    int ret = 0;
    ret = i2c_add_driver(&csa37f71_driver);
    return ret;
}

static void __exit csa37f71_exit(void)
{
    LOG_DEBUG("module exit\n");
    i2c_del_driver(&csa37f71_driver);
    cs_procfs_delete();
}

module_init(csa37f71_init);
module_exit(csa37f71_exit);
MODULE_LICENSE("GPL");
