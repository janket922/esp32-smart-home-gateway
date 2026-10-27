# ESP32-S3 智能家居网关

基于 ESP-IDF 的智能家居固件：通过 OV2640 DVP 摄像头采集 JPEG，编码为 Base64 JSON 后经 MQTT 上传；通过 TWAI/CAN ISO-TP 接收传感器数据并上传温度、湿度和照度。支持按键控制视频采集。

## 硬件与开发环境

- ESP32-S3，16 MiB Flash，八线 PSRAM。
- OV2640 DVP 摄像头、XL9555 IO 扩展器、按键和 LED。
- CAN 收发器与匹配协议的传感器发送端。
- 本次验证使用 ESP-IDF **v6.0.1**。引脚与摄像头、PSRAM、分区设置保存在 `sdkconfig.defaults`；换板前请按原理图核对。

## 本机凭据

版本库中的 `Components/Svc/Svc_Mqtt/Svc_User_Defined.h` 使用 `XX` 占位。真实配置放在同目录的 **`Svc_User_Defined.local.h`**，该文件已被 `.gitignore` 排除。

首次配置时，从公共头文件复制 `#else` 下的宏定义到本地文件，填写 Wi-Fi、MQTT URI、设备 ID、用户名、密码和发布/订阅主题。请为本地文件使用独立的头文件保护宏，例如 `SVC_USER_DEFINED_LOCAL_H`。编译器会自动优先加载本地文件。

原有开发机已保留本地配置；克隆仓库不会获得真实密码。未填写时仍可编译，但不能连接 Wi-Fi 或云平台。可定义 `SVC_USE_PLACEHOLDERS=1` 强制使用占位配置进行构建验证。

MQTT 是否使用 TLS 由 URI 的 `mqtt://` 或 `mqtts://` 决定；使用 TLS 时已配置证书包验证，请按云平台提供的连接参数配置。

## 构建与烧录

在已激活 ESP-IDF v6.0.1 的终端执行：

```sh
idf.py set-target esp32s3
idf.py build
idf.py -p PORT flash monitor
```

将 `PORT` 替换为实际串口。配置调整可使用 `idf.py menuconfig`。首次构建会下载依赖，厂商摄像头与视频组件源码已随项目保留；生成的 `managed_components` 和构建目录不提交。

## 代码结构

| 目录 | 职责 |
| --- | --- |
| `main` | 系统启动与业务任务初始化 |
| `Components/App` | 视频状态机、传感器同步、云端上传 |
| `Components/Svc` | 帧池与所有权、Base64/JSON、MQTT、ISO-TP |
| `Components/Bsp` | 摄像头、Wi-Fi、按键、LED、XL9555 与厂商组件 |
| `verification/2026-10-28-review` | 本次修复后的验证脚本与结果 |
| `reference` | 历史方案参考，不参与固件构建 |

## 验证范围

本次修复覆盖 Base64 上限越界、摄像头映射和复制边界、MQTT 空指针、Wi-Fi 回调配置生命周期、传感器 JSON 长度检查及 NVS 启动恢复。

验证包含完整 ESP32-S3 固件构建、13 个自有 C 文件的独立交叉编译，以及 43 项主机检查。主机检查使用 RTOS/驱动替身，不能替代开发板上的摄像头、CAN、Wi-Fi 和云平台联调。详细结果见 `verification/2026-10-28-review/检查结果.md`；较早日期的报告保留为历史记录。

`pytest_mqtt_ssl.py` 是项目起始模板遗留的 MQTT SSL 示例测试，不适用于当前智能家居业务的串口验收。

厂商组件的许可证保留在各自目录中。
