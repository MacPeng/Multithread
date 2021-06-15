//I cooperated with Yan Wen in this assignment and we talked about the sturctures of several functions.
//And we watched a video which explains the ideas of this assignment.
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>

#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <sstream>
#include <deque>

#include "HTTPRequest.h"
#include "HTTPResponse.h"
#include "HttpService.h"
#include "HttpUtils.h"
#include "FileService.h"
#include "MySocket.h"
#include "MyServerSocket.h"
#include "dthread.h"

using namespace std;

int PORT = 8080;
int THREAD_POOL_SIZE = 200;
unsigned int BUFFER_SIZE = 1;
string BASEDIR = "static";
string SCHEDALG = "FIFO";
string LOGFILE = "/dev/null";

pthread_mutex_t mutexes = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t requests  = PTHREAD_COND_INITIALIZER;
pthread_cond_t room  = PTHREAD_COND_INITIALIZER;
deque<MySocket*> buffer;
vector<HttpService *> services;

HttpService *find_service(HTTPRequest *request) {
   // find a service that is registered for this path prefix
  for (unsigned int idx = 0; idx < services.size(); idx++) {
    if (request->getPath().find(services[idx]->pathPrefix()) == 0) {
      return services[idx];
    }
  }

  return NULL;
}


void invoke_service_method(HttpService *service, HTTPRequest *request, HTTPResponse *response) {
  stringstream payload;

  // invoke the service if we found one
  if (service == NULL) {
    // not found status
    response->setStatus(404);
  } else if (request->isHead()) {
    payload << "HEAD " << request->getPath();
    sync_print("invoke_service_method", payload.str());
    cout << payload.str() << endl;
    service->head(request, response);
  } else if (request->isGet()) {
    payload << "GET " << request->getPath();
    sync_print("invoke_service_method", payload.str());
    cout << payload.str() << endl;
    service->get(request, response);
  } else {
    // not implemented status
    response->setStatus(405);
  }
}



void handle_request(MySocket *client) {
  HTTPRequest *request = new HTTPRequest(client, PORT);
  HTTPResponse *response = new HTTPResponse();
  stringstream payload;

  // read in the request
  bool readResult = false;
  try {
    payload << "client: " << (void *) client;
    sync_print("read_request_enter", payload.str());
    readResult = request->readRequest();
    sync_print("read_request_return", payload.str());
  } catch (...) {
    // swallow it
  }

  if (!readResult) {
    // there was a problem reading in the request, bail
    delete response;
    delete request;
    sync_print("read_request_error", payload.str());
    return;
  }

  HttpService *service = find_service(request);
  invoke_service_method(service, request, response);

  // send data back to the client and clean up
  payload.str(""); payload.clear();
  payload << " RESPONSE " << response->getStatus() << " client: " << (void *) client;
  sync_print("write_response", payload.str());
  cout << payload.str() << endl;
  client->write(response->response());

  delete response;
  delete request;

  payload.str(""); payload.clear();
  payload << " client: " << (void *) client;
  sync_print("close_connection", payload.str());
  client->close();
  delete client;
  return;
}

//consumer
// This is used to handle first request in buffer queue
void *threads(void* arg){
    while(true){
        dthread_mutex_lock(&mutexes);
        while(buffer.size() == 0){
            dthread_cond_wait(&requests, &mutexes);
        }
        
        //put client into queue
        MySocket *client = buffer.front();
        // remove first element in queue
        buffer.pop_front();
        //passing  function
        dthread_cond_signal(&room);
        dthread_mutex_unlock(&mutexes);
        //handle client request if this passes
        if (client){
            handle_request(client);
        }
    }
}

//producer
void * make_new_threads(){
    pthread_t  * newThread = new pthread_t[THREAD_POOL_SIZE];
    for (int i = 0; i < THREAD_POOL_SIZE; i++){
        dthread_create(&newThread[i], NULL, threads, NULL);
    }
    return 0;
}

int main(int argc, char *argv[]) {

  signal(SIGPIPE, SIG_IGN);
  int option;

  while ((option = getopt(argc, argv, "d:p:t:b:s:l:")) != -1) {
    switch (option) {
    case 'd':
      BASEDIR = string(optarg);
      break;
    case 'p':
      PORT = atoi(optarg);
      break;
    case 't':
      THREAD_POOL_SIZE = atoi(optarg);
      break;
    case 'b':
      BUFFER_SIZE = atoi(optarg);
      break;
    case 's':
      SCHEDALG = string(optarg);
      break;
    case 'l':
      LOGFILE = string(optarg);
      break;
    default:
      cerr<< "usage: " << argv[0] << " [-p port] [-t threads] [-b buffers]" << endl;
      exit(1);
    }
  }

  set_log_file(LOGFILE);

  sync_print("init", "");
  MyServerSocket *server = new MyServerSocket(PORT);
  MySocket *client;
  services.push_back(new FileService(BASEDIR));
  make_new_threads();

// push requests to queue
  while(true) {
      sync_print("waiting_to_accept", "");
      client = server->accept();
      sync_print("client_accepted", "");
      dthread_mutex_lock(&mutexes);
      //use while surrounding wait condition
      while(buffer.size() == BUFFER_SIZE){
          dthread_cond_wait(&room, &mutexes);
    }
// push queue
      buffer.push_back(client);
//fifo signal
      dthread_cond_signal(&requests);
      dthread_mutex_unlock(&mutexes);
   }
    
}

