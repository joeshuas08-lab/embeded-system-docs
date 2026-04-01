---
tags:
  - 产品分析
---
本文分析国际友商们的demo的优势和劣势，针对提出我们如何差异化竞争的方向

---

## 1. TRIA

### Demo优点：

1. Custom system solutions 丰富
		-数量10+
		-来源于其客户定制案例（更有说服力、且是现成的）；

2. HMI solution 丰富
	   -涵盖 7inch~12inch，
	   -源于成功的custom carrier board案例，
	   -因此也同时标了具体行业信息（for /Agriculture Machinery/industrial automation/Heavy Road Machinery）；

### 其他优点：

1. 本地化能力
	-德国本地研发+支持团队、
	-亚洲大陆有销售办公室；

2. 联合原厂展示；![](Pasted%20image%2020260323195321.png)


### Demo缺点：

1. Demo以Custom system solutions 仅硬件静态展示和文字描述
	无软件互动等效果展示；
	

2. Robotic powered by Ai 的demo仅展示在Qualcomm等高算力平台 , 
	无RK、AW平台；

### 对策：

- 在RK、AW平台实现差异化竞争，代替部分NXP平台，在国外同等算力级别的市场；  
    -这个可能需要有10~20 dollar的价格优势？
    -因为这个算力级别，如果只有 几dollar的价格优势、代理反馈客户宁愿用原来贵的也不愿意换；


---


## 2. Advantech（No.1）

### Demo优点：

1. 产品线极全
- SOM / SBC / IPC box / Edge Server
		- 其中SOM涵盖- SMARC、 Qseven COM 、Express、OSM封装
  ![672](4b2f9ed8eab5f787023bdb5a8371ea8d.jpg)

  2. 覆盖行业范围全
- Medical Devices / Industrial Automation / Energy / Intelligent Transportation/ IoT/ HMI / Edge Server等等全行业范围

 1. Demo/产品有rk3588
	-现在很多客户在用RK3588
	-也有很多客户问我们有没有RK3588方案，这类客户画像主要集中在中高端图像处理
	-因为在同一价位上，RK3588在中高端视频处理（encode 8K@30fps、decode 8K@60fps、48M ISP素）的能力无其他Soc可替代;

### 其他优点：

1. 全球渠道能力强
- 区域性服务很好，展会现场基本欧洲当地的销售和支持团队；

### Demo&产品&服务缺点：

1. SKU（Stock Keeping Unit）过多 → 复杂;  

2. 对10k unit级别的客户定制慢;  

3. 性价比一般;


### 对策

 - 服务制胜
	 -“我们比他更上心的Technical support，在10k unit这个量级上“
	 -”我们是原厂的战略合作伙伴，技术支持足够专业“；



---


## 3. Toradex

### Demo优点：

- 完善的软件体系（__Torizon__ / OTA / container）；

- 灵活的教学评估板支持板载各个模块插拔；
	
- 完善的硬件画图提示软件（应该不是IDE）；

- 针对某个应用场景，有丰富的流程图说明；

- 提供能应对CRA的系统架构能力支持，CRA合规认证是需要终端产品完成；
	-Torizon系统部分功能：支持系统层软件OTA A/B分区更新、软件加密；
	-CRA合规认证是需要终端产品完成；
	-CRA要求为每件产品加贴CE标识（制造商需为每件含数字元素的产品加贴CE标志，以显示产品符合CRA规定的要求）
	
- 有i.mx95 smarc；
  
### Demo缺点：

- 成本高

- 不够灵活

- AI性能不激进

### 产品特点：

- i.mx 95 smarc 250-280 usd

- 当下的价格 valid for twodays

- 还有库存 lead time 4~7周



---


## 4. Phytech


### Demo优点：

- 丰富针对性行业案例
		- 双开展台两侧5+5案例
		- 每个案例配明确流程图
- 强调CRA合规，放在公司标题旁边
		- 宣传册配备明确流程图直观解析  如何改进操作系统应对CRA
- 具有通用测试平台 可替换不同核心板进行功能验证
		- 同一测试基板，对不同核心板的兼容性约为60%；

### Demo缺点：



### 产品特点：

- 但是实际上还正在做关于CRA的调查，还没有take action；

- i.mx 91 SOM 价格涨到60~70美金；

- 样品有库存 批量lead time 4~7周


---


## 5. Karo

### Demo优点：
- 未明确

### 产品优点：

- 有足够到年底的库存

- 有当地代理帮忙推广，因此年销量客观；

### Demo缺点：

- 规模相对小，展位上仅有两个系列产品（Renesas、ST）

- 仅有的demo为 HMI 和 st原厂的ai demo

### 产品缺点：

- 价格高（52~62美金 for 1k pcs)

- 交期8~12周

- 自己定义的封装



---
## Kontrol

### Demo优点：

- 超级强调CRA

### Demo缺点：

- 成本高


### 对策：

- CRA对策一定要持续推进
	-现场客户约有一半知道CRA；
	    - 其中个位数的客户看了我们ST257的CRA的策略，但未有实际反馈；



---
## TechNexion

### 产品的特点；

- 采用自己的封装,

- 存储芯片都是买现货所以库存，所以核心板模块价格太高属于停摆（出货量比之前少50%），

- 想要和我们合作：搭配我们RK开发板适配做AR系列的摄像头;

### Demo特点：

- 每个demo是一个核心板模块配合一个自家摄像头模块


### 对策：

- 是否考虑合作？目前我们已在RK上适配过AR系列摄像头模组；
		- 合作的话他们模组自带ISP，所以我们只需要调通链路，而无需调试ISP；