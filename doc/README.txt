
knotty 项目包含 dlog日志系统，xic远过程调用(RPC)框架(C++及php客户端)，以及x4fcgi程序（用fcgi来实现xic服务）。
knotty 目前只支持Linux系统，推荐CentOS或Ubuntu。


================================
 编译 dlog, xic 及 x4fcgi
================================

$ git clone https://github.com/halftwo/knotty 
$ cd knotty
$ make

如果以上make没有出现什么错误，那么
在 dlog 目录下，会得到编译好的 dlogd, dlog_center 及 dlog_write 程序。
在 xic 目录下，会得到编译好的 shadow_gen, secret_check，及演示程序 DemoServer 和 DemoClient等。
在 x4fcgi 目录下，会得到编译好的 x4fcgi 程序。


============
 运行 dlog
============

将 dlogd, dlog_center, dlog_write 安装(拷贝)到目标目录（你期望的任意目录）

 运行dlog_center
-----------------
$ ./dlog_center /path/to/dlog
其中，/path/to/dlog是你想让日志保存的目录。
dlog_center 将会以daemon方式运行，并且监听所有地址的TCP6108端口。


 运行dlogd
-----------------
$ ./dlogd

dlogd 将会以daemon方式运行，监听loopback地址TCP及UDP的6109端口，并且尝试连接本地的dlog_center的6108端口。
需要写dlog日志的程序，将日志发送给本地的UDP或TCP6109端口(dlogd监听)，dlogd收到后将日志批量转发给dlog_center，
最后dlog_center将日志记录到文件中。

dlogd 程序在运行过程中，会定期检测它所在目录下的 dlogd.plugin 文件。如果该文件有更新，则读取该文件并执行该文件中的lua程序。

源文件knotty/dlog目录中，有一个 dlogd.plugin 文件作为实例。其中
dlog.set_center("127.0.0.1")
语句指定了该 dlogd需要连接的 dlog_center 的地址。每个 dlogd只能连接一个dlog_center。


 测试dlog系统
-----------------
$ ./dlog_write XXX "hello, world" 

查看 /path/to/dlog 目录下的 zlog 文件，将会看到类似下面的内容

>190314r170109 172.27.1.10 24994+56401 dlog_write XXX dlog_write.c:41 hello, world



===========================
 xic的演示程序 DemoServer
===========================

xic是一个远过程调用(RPC)框架，xic目录中，除了库libxic.a外，我们还提供了演示程序。
DemoServer是服务器端的演示程序，DemoClient是客户端的演示程序。
DemoServer的启动方法是
$ ./DemoServer --xic.conf=conf.demo

其中，conf.demo是它的配置文件。
conf.example 是一个配置文件的样本，它包含了所有可能的配置项（及其默认值）。

DemoClient的运行方法是
$ ./DemoClient localhost+55555

其中，localhost+55555 是服务器端的endpoint(地址及端口)，需要注意的是在xic中，地址和端口之间用加号(+)来分割而不是用冒号(:)。
如果以上命令打印出类似以下内容，则表示演示程序（服务器端和客户端）正确运行。

----------------------------- echo
a = hello, world!
b = 529280586
c = 0.357614
----------------------------- time
  con=tcp/127.0.0.1+55555/127.0.0.1+56748
 time=1552553246
  utc=190314r084726
local=190314r164726



=======================================
 编译xic的php客户端（php扩展 xic.so）
=======================================

以php7为例。首先，进入 php7_xic 目录。然后，根据该目录下的文件 INSTALL 中的指令进行编译安装配置等操作。
如果系统中有多个版本的php，需要注意编译安装过程中使用phpize, php-config及php程序的版本要一致。


===================
 运行 x4fcgi 程序
===================

参看 knotty/x4fcgi 目录下的 INSTALL 文件


===================
 xic配置文件说明
===================

注意：xic的配置文件改动之后必须重启程序才能生效。

 线程池配置
--------------
默认的配置如下,
xic.PThreadPool.Server.Size = 1
xic.PThreadPool.Server.SizeMax = 1
xic.PThreadPool.Client.Size = 1
xic.PThreadPool.Client.SizeMax = 1
通常需要将其修改为更大的值，比如CPU核心数的两倍。


 日志配置
-----------------
以下配置记录慢请求(服务器端看到的和客户端看到的)到日志。如果请求执行的时间超过了配置指定的值（以毫秒为单位），
则将该请求记录到dlog日志里。-1标志不记录任何慢请求，0表示记录所有请求（因为所有请求的执行时间都大于等于0毫秒）。
xic.slow.server = -1
xic.slow.client = -1

以下配置按概率抽样记录请求。如果配置值为0，则不记录。任何大于0的值N, 都按照1/N的概率进行抽样记录到dlog日志里。
xic.sample.server = 0
xic.sample.client = 0


 用户配置
------------------
以下配置xic程序执行进程的用户名和组名，为了安全，通常长时间运行的服务程序都使用权限受限的用户来执行，特别是不能用
root来执行。
xic.user = root
xic.group = root



===================
 xic调用安全
===================

为了防止xic服务被恶意调用，我们采用两种方式来进行限制：1限制调用者的IP，2调用者必须提供密码。
这两种限制都是在连接是进行检查的，并不是每次请求进行检查。也就是说，一旦通过了连接时的检查，后面所有的请求都不再进行安全检查了。

 限制调用者IP
--------------
限制调用者IP只需要进行如下配置就可以了,
xic.allow.ips = 127.0.0.1 ::1 192.168.1.0/24


 密码限制
-------------------
如果我们采用密码限制，则服务器端需要配置
xic.passport.shadow = shadow.example
xic.cipher = AES128-EAX

其中，
xic.passport.shadow 的值是一个文件名，该文件中保存了客户端用户密码的verifier（密码经过单向运算的值）
xic.cipher 的值是信道加密的方法（如果采用密码限制，则信道默认是加密的，可以不指定此项）

shadow文件的条目(verifier)可以通过 shadow_gen 程序产生，提供一些参数和用户名及密码，shadow_gen就会将该条目(verifier)的内容生成，
运维人员只需将该条目拷贝到 shadow文件中即可，该条目将会在20秒内生效。


客户端需要配置
xic.passport.secret = secret.example

其中，
xic.passport.secret 的值是一个文件名，该文件中保存了客户端的用户名和密码(明文)。
由于一个客户端可能访问多个不同的服务器端，所以该文件可能保存了多个不同的用户名和密码。


客户端和服务器端采用SRP6a来验证密码，期间不会有密码明文信息在网络上传递。


通常，用户名及密码都是用shadow_gen随机产生，运维人员需要定期更换xic调用的用户名和密码。
更换的过程如下:
a. 用shadow_gen产生一对新的用户名及密码，并将对应的verifier拷贝到shadow文件中;
b. 等20秒让上述修改生效;
c. 将上述新用户名及密码替换secret文件中的老用户名及密码;
d. 等20秒让上述修改生效;
e. 将shadow文件中老用户名及密码对应的verifier删除即可。


