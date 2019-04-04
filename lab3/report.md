# lab1 - 516030910391 徐天强

### task 1: implement a meter and a dropper

1. ```c++
   int qos_meter_init(void)
   ```

   ​	在此函数内，执行meter的初始化逻辑。APP_FLOWS_MAX，表示我们的flow数目，我们对每一个flow，调用rte_meter_srtcm_config，来配置我们srTCM的相关参数。我们的参数、由rte_meter_srtcm_params提供。其具体初始化代码如下：

   ```c++
   struct rte_meter_srtcm_params app_srtcm_params[] = {
   	{.cir = 1000000 * 46,  .cbs = 2048, .ebs = 2048},
   };
   ```

   ​	其中，cir是committed information rate，cbs是committed burst size，ebs是excess burst size。

   ​	qos_meter_init的完整代码如下：

   ```c++
   int
   qos_meter_init(void)
   {
       uint32_t i, j;
   	int ret;
   	for (i = 0, j = 0; i < APP_FLOWS_MAX;i ++, j = (j + 1) % RTE_DIM(app_srtcm_params)) {
   		ret = rte_meter_srtcm_config(&qos_flows[i], &app_srtcm_params[j]);
   		if (ret)
   			return ret;
   	}
   	return 0;
   }
   ```

2. ```c++
   enum qos_color qos_meter_run(uint32_t flow_id, uint32_t pkt_len, uint64_t time)
   ```

   ​	此函数被主函数调用，由pkt_len来推测当前包的类别。执行分类逻辑的函数是rte_meter_srtcm_color_blind_check，这个函数是meter的核心函数。我们的qos_meter_run仅是对其的一层封装。

   ​	qos_meter_run调用rte_meter_srtcm_color_blind_check的具体过程如下：

   ```c++
   enum qos_color
   qos_meter_run(uint32_t flow_id, uint32_t pkt_len, uint64_t time)
   {
       enum qos_color color;
   	color = (enum qos_color) rte_meter_srtcm_color_blind_check(&qos_flows[flow_id],
               time, pkt_len);
   	return color;
   }   
   ```

3. 为了基于WRED算法，实现我们的dropper， 需要定义一些全局变量： 

   ```c++
   struct rte_sched_queue *qos_queue[COLOR_NUM];
   struct rte_red_config *qos_cfg[COLOR_NUM];
   struct rte_red *qos_red[COLOR_NUM];
   
   	首先，第一个qos_queue，是自定义的数据结构，只包含一个uint16_t类型的queue_size成员变量。
   struct rte_sched_queue {
   	uint16_t size;
   };
   	接下来是qos_cfg指针，它指向一个RED的配置参数数组。具体数据结构可在/lib/librte_sched/rte_red.h文件中查看。
   	最后是qos_red指针，指向rte_red数组，是WRED核心的数据结构。具体数据结构可在/lib/librte_sched/rte_red.h文件中查看。
   ```

4. ```c++
   int qos_dropper_init(void)
       该函数用于对我们实现的dropper进行初始化。由于我们定义了三个全局数据结构，因此需要对三个全局变量分别进行初始化。对于rte定义的两个数据结构，我们分别调用了rte定义的两个init函数。
       int rte_red_rt_data_init(struct rte_red *red)
       int rte_red_config_init(struct rte_red_config *red_cfg,	const uint16_t wq_log2, const uint16_t min_th, const uint16_t max_th, const uint16_t maxp_inv)
   ```

5. ```c++
   int qos_dropper_run(uint32_t flow_id, enum qos_color color, uint64_t time)
       该函数被main函数调用，作为运行时对包执行drop的逻辑。我们根据flow_id对包进行分类，使用不同的配置参数来执行drop逻辑。color通用是用来分类的。time由main函数进行管理，每次刷新time，意味着已经过去相当长的时间，我们需要显示的清楚queue中的所有包，并将size置零。
   static inline int rte_red_enqueue(const struct rte_red_config *red_cfg,	struct rte_red *red, const unsigned q, const uint64_t time)
       其中丢包的核心逻辑通过这个函数进行实现。它返回一个int值，若是0则表示不丢包。如果是1或者2，则执行丢包。
   ```

### task 2: deduce parameters

1. 在这个task中，我们需要实现的是对不同的flow，设置不同的bandwidth。为了满足这个，我们对不同flow配置不同的WRED config参数。

   ```c++
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
       通过修改MIN_TH，MAX_TH和MAXP_INV三个参数，是的最终我们的实现达到8：4：2：1的bandwidth限制。
   
   ```
   

![res](D:\course\2019spring-大三下\distributed system\labs\lab3\result.jpg)