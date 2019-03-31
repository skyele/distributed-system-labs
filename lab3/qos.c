#include "rte_common.h"
#include "rte_mbuf.h"
#include "rte_meter.h"
#include "rte_red.h"

#include "qos.h"

#define COLOR_NUM 3
#define FLOW_NUM 4

#define WQ_LOG2 7 // [1, 12]
#define MIN_TH 0
#define MAX_TH 1080
#define MAXP_INV 150 // [1, 255]

uint64_t oldtime = 0;

struct rte_sched_queue {
	uint16_t size;
};

struct rte_meter_srtcm qos_flows[APP_FLOWS_MAX];
// struct flow_conf qos_conf[MAX_DATA_STREAMS];

struct rte_sched_queue *qos_queue[COLOR_NUM];
struct rte_red_config *qos_cfg[COLOR_NUM];
struct rte_red *qos_red[COLOR_NUM];

struct rte_meter_srtcm_params app_srtcm_params[] = {
	{.cir = 1000000 * 46,  .cbs = 2048, .ebs = 2048},
};

struct rte_meter_trtcm_params app_trtcm_params[] = {
	{.cir = 1000000 * 46,  .pir = 1500000 * 46,  .cbs = 2048, .pbs = 2048},
};

/**
 * srTCM
 */
int
qos_meter_init(void)
{
    uint32_t i, j;
	int ret;

	for (i = 0, j = 0; i < APP_FLOWS_MAX;
			i ++, j = (j + 1) % RTE_DIM(app_srtcm_params)) {
		ret = rte_meter_srtcm_config(&qos_flows[i], &app_srtcm_params[j]);
		if (ret)
			return ret;
	}

	return 0;
}

enum qos_color
qos_meter_run(uint32_t flow_id, uint32_t pkt_len, uint64_t time)
{
    enum qos_color color;

	color = (enum qos_color) rte_meter_srtcm_color_blind_check(&qos_flows[flow_id],
                time, pkt_len);

	return color;
}


/**
 * WRED
 */

int
qos_dropper_init(void)
{
    /* to do */
	for(int i = 0; i < COLOR_NUM; i++){
		qos_queue[i] = (struct rte_sched_queue *)malloc(sizeof(struct rte_sched_queue));
		qos_queue[i]->size = 0;
		qos_red[i] = (struct rte_red *)malloc(sizeof(struct rte_red));
		rte_red_rt_data_init(qos_red[i]);
		// qos_cfg[i] = (struct rte_red_config *)malloc(sizeof(struct rte_red_config));
		// rte_red_config_init(qos_cfg[i], WQ_LOG2, MIN_TH, MAX_TH, MAXP_INV);
	}

	for(int i = 0; i < FLOW_NUM; i++){
		qos_cfg[i] = (struct rte_red_config *)malloc(sizeof(struct rte_red_config));
		if(i == 0)
			rte_red_config_init(qos_cfg[i], WQ_LOG2, MIN_TH, MAX_TH * 1/2, MAXP_INV);
		else if(i == 1)
			rte_red_config_init(qos_cfg[i], WQ_LOG2, MIN_TH, MAX_TH * 1/4, MAXP_INV);
		else if(i == 2)
			rte_red_config_init(qos_cfg[i], WQ_LOG2, MIN_TH, MAX_TH * 3/19, MAXP_INV);
		else
			rte_red_config_init(qos_cfg[i], WQ_LOG2, MIN_TH, MAX_TH * 3/23, MAXP_INV);
		
	}

	return 0;
}

int
qos_dropper_run(uint32_t flow_id, enum qos_color color, uint64_t time)
{
	if(oldtime != time){
		for(int i = 0; i < COLOR_NUM; i++){
			qos_queue[i]->size = 0;
		}
		oldtime = time;
	}
	struct rte_red_config *red_cfg;
	struct rte_red *red;
	red_cfg = qos_cfg[flow_id];
	if ((red_cfg->min_th | red_cfg->max_th) == 0){
		return 0;
	}
	red = qos_red[color];
	int isEnqueue = rte_red_enqueue(red_cfg, red, qos_queue[color]->size, time);
	if(isEnqueue == 0){
		qos_queue[color]->size++;
	}
	return isEnqueue;
}