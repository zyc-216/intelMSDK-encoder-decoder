# intelMSDK-encoder-decoder
  Using intel media sdk and intel GPU(hardware),encode nv12 to Jpeg/H264 and decode Jpeg/H264 to nv12.

  Using the intel media sdk library, the video/picture encoding and decoding classes are encapsulated, and the programming language is c++. The encapsulated encoder and decoder need to instantiate the corresponding parameter classes before use. At the same time, the provided code encapsulates the class that reads data from the sqlite database and writes it into avi format, and the class that converts h264 frame data into avi video format. Both encoding and decoding are done in hardware, using Intel GPU.
  Examples of use are not given now. But will be later.
  
    使用intel media sdk库，封装了视频/图片编码和解码的类，编写语言为c++，封装的编码器和解码器在使用前需要实例化相应的参数类。同时，提供的代码封装了从sqlite数据库中读取数据并写成avi格式的类以及由h264帧数据到avi视频格式的类。编码和解码都是硬件方式，使用Intel GPU。
    使用实例暂未给出。
