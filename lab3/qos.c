#include "rte_common.h"
#include "rte_mbuf.h"
#include "rte_meter.h"
#include "rte_red.h"

#include "qos.h"

#define FLOW_METER   struct rte_meter_srtcm

FLOW_METER qos_flows[APP_FLOWS_MAX];

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
}

int
qos_dropper_run(uint32_t flow_id, enum qos_color color, uint64_t time)
{
    /* to do */
}