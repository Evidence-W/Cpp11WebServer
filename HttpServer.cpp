#include "HttpServer.h"
#include <cassert>

using namespace swings;

HttpServer::HttpServer(int port) 
    : port_(port),
      listenFd_(createListenFd(port_))
{
    assert(listenFd_ >= 0);
}

HttpServer::~HttpServer()
{
}

void HttpServer::run()
{
    // 注册监听套接字的可读事件，ET模式
    HttpRequest listenRequest(listenFd_);
    epoll_.add(listenFd_, &listenRequest, (EPOLLIN | EPOLLET));

    // 注册新连接回调函数
    epoll_.setOnConnection(std::bind(&HttpServer::acceptConnection, this));

    // 事件循环
    while(1) { // FIXME 服务器应该能够停止
        int eventsNum = epoll_.wait(TIMEOUTMS);
        epoll_.handleEvent(listenFd_, eventsNum);
    }
}

void HttpServer::acceptConnection()
{
    // 接受新连接 
    struct sockaddr_in clientAddr;
    memset(&clientAddr, 0, sizeof(struct sockaddr_in));
    socklen_t clientAddrLen = 0;
    int acceptFd = ::accept(listenFd_, (struct sockaddr*)&clientAddr, &clientAddrLen);

    // accept系统调用出错
    if(acceptFd == -1) {
        perror("accept");
        return;
    }

    // 设置为非阻塞
    if(setNonBlocking(acceptFd) == -1) {
        ::close(acceptFd); // 此时acceptFd还没有HttpRequst资源，不需要释放
        return;
    }

    // 用shared_ptr管理连接套接字
    HttpRequestPtr conn(new HttpRequest(acceptFd));
    requests_.push_back(conn);
    
    // 注册连接套接字到epoll
    // 可读，边缘触发，保证任一时刻只被一个线程处理
    epoll_.add(acceptFd, conn.get(), (EPOLLIN | EPOLLET | EPOLLONESHOT)); 
}

