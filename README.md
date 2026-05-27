# IMX6ULL 智能监控演示

这个演示程序将 OV5640 的预览画面改造成一个偏监控终端风格的 Qt 控制台界面，而不再是一个基础的摄像头测试窗口。

## 当前功能

- 通过 `/dev/video1` 进行 V4L2 采集
- 在 Qt Widgets 界面中提供 `800x480` 实时预览
- 使用与当前已验证开发板环境一致的 RGB565 采集路径
- 监控终端风格布局，包括：
  - 大尺寸预览区域
  - 系统状态面板
  - 事件日志
  - 启动 / 停止 / 抓拍控制
- 人脸录入示例按钮
- 帧计数与近似 FPS 显示
- 模拟人脸处理流程：绘制测试框并输出人脸数量更新
- 为降低嵌入式开发板上的 UI 内存压力，预览刷新做了限速处理
- 视觉处理流程已通过接口解耦，便于后续替换为真实检测器

## 这个版本为什么有用

- 相比单纯的预览窗口，它更接近真实的嵌入式监控产品
- UI 已经预留了清晰的状态、告警和识别结果展示区域
- 采集线程中已经有 `runVisionPipeline()` 钩子，无需重构主循环就能加入人脸逻辑
- 当前的模拟检测器可以帮助你在接入真实算法库之前，先验证叠加绘制、事件流和样本保存流程
- 现在替换检测器时，只需要新增一个 `VisionPipeline` 实现，而不必重写摄像头工作线程

## 编译

在开发板上并且已经具备 Qt 开发工具时执行：

```sh
qmake
make
```

如果你是在 PC 上进行交叉编译，请使用与开发板匹配的 Qt 工具链。

## 运行

```sh
chmod +x imx6ull_ov5640_qt_demo
./imx6ull_ov5640_qt_demo
```

## 浏览器预览

应用启动后，还会同时启动一个轻量级网页预览服务器，地址为：

```txt
http://<board-ip>:8080
```

浏览器页面支持：

- 通过 MJPEG 流进行实时预览
- 抓拍按钮
- 开始录制
- 停止录制

输出路径：

- 网页抓拍图片：`/home/web_snapshots`
- 网页录制帧：`/home/web_recordings`

当前录制功能为了兼容开发板环境，采用 JPEG 帧序列的方式保存，而不是编码后的 mp4 文件。

远程浏览器预览会有意降低分辨率并进行 JPEG 压缩，以便将开发板侧 CPU 负载控制在低于本地屏幕预览路径的水平。

## 离线视觉测试

你可以在不打开摄像头的情况下测试 `RealVisionPipeline`：

```sh
./imx6ull_ov5640_qt_demo --offline-test input.jpg output.jpg
```

该模式会：

- 加载 `input.jpg`
- 运行 `RealVisionPipeline`
- 将带标注的结果图保存为 `output.jpg`
- 在终端输出人脸数量和检测详情

## 当前模拟行为

- 在预览画面上绘制模拟人脸框
- 更新 `Faces` 状态字段
- 将检测变化写入事件日志
- 将录入样本保存到 `/home/face_samples`

## 源码结构

- `cameraworker.*`：摄像头采集、预览调度、录入样本保存
- `visionpipeline.h`：通用检测器接口
- `facedetectorbackend.h`：通用检测后端接口
- `mockvisionpipeline.*`：当前模拟检测器实现
- `realvisionpipeline.*`：后续接入真实检测器的预留实现
- `simplefacedetectorbackend.*`：基于图像内容的轻量级人脸候选区域检测后端

## 流水线选择

默认情况下，应用使用 `mock` 流水线。

如果存在 `/home/imx6ull_vision_mode.conf`，并且其内容为：

```txt
real
```

应用将切换到 `RealVisionPipeline`。

目前 `RealVisionPipeline` 使用的是一个内置的轻量级后端，并会输出：

- 基于图像内容得到的类人脸候选框
- 后端状态信息，例如 `simple-skin-detector: 1 face candidate(s)`

这样你可以先把整体架构打通，后续再接入真实检测器，而无需修改 UI 或摄像头工作线程流程。

## 真实检测器接入点

当你准备接入真实算法库时，请修改以下位置：

- `RealVisionPipeline::prepareInputFrame()`
  用于像素格式转换、缩放和归一化。
- `RealVisionPipeline::inferDetections()`
  在这里调用你的检测后端，并返回 `FaceDetection` 结果。
- `RealVisionPipeline::drawDetections()`
  如有需要，可保留或调整叠加框的绘制风格。
- 后续将 `SimpleFaceDetectorBackend` 替换为真正的模型后端。

检测器应当填充以下字段：

- `FaceDetection::box`
- `FaceDetection::label`
- `FaceDetection::confidence`
- `FaceDetection::recognized`

## 真实人脸识别的下一步

推荐路径如下：

1. 保留当前监控界面 UI
2. 在 `RealVisionPipeline::process()` 中实现真实检测流程
3. 在 `inferDetections()` 中返回标准化的 `FaceDetection` 条目
4. 增加一个本地小型人脸库，用于录入和匹配

## 建议验证流程

1. 先在 `mock` 模式下运行 GUI，确认监控界面仍然正常
2. 使用示例图片运行 `--offline-test`
3. 在 `inferDetections()` 中实现真实检测逻辑
4. 反复运行 `--offline-test`，直到输出图片和终端结果都正确
5. 将 `/home/imx6ull_vision_mode.conf` 切换为 `real`
6. 测试实时摄像头路径

## 重要说明

当前的 `real` 模式只是一个用于验证流程的轻量级检测器，并不是可用于生产环境的人脸识别器。它可以高亮类似人脸的肤色区域，但不能识别人是谁，也可能出现误检或漏检。

如果 RGB565 对检测器不方便，建议将摄像头输入切换为 `YUYV`，并在工作线程中完成颜色空间转换。
