# YdgodOS
一个基于stm32单片机的微型操作系统

# v1.0
## 版本特性
- 采用协作式调度，由任务主动释放cpu
- 执行模拟iic读取陀螺仪数据+mahony+串级pid运算时，任务循环率可达到1.1khz
# 任务状态转换图：
<img width="957" height="586" alt="新建 BMP 图像" src="https://github.com/user-attachments/assets/c0c2aed9-7928-4031-8d0d-cb49805b8c3b" />

- 1 由os_tick_handler 遍历任务数组 根据task.tick是否达到task.reload，从而将任务由等待态转向就绪态
- 2 当前一个task释放cpu之后  由Scheduler 调度下一个task ，并将task从就绪态转为运行态
- 3  task释放cpu之后 ,返回Scheduler，由Scheduler将task转为等待态
## 下一个版本预告
- 4、5将会在下一个版本中实现，目标是使用时间片轮转调度，实现更高的调度效率
