/*
 *
 * Copyright 2015, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

// Homework 4
// Colin Banigan and Katherine Drake
// CSCE 438 Section 500
// April 14, 2017

#include <ctime>

#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/duration.pb.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <stdlib.h>
#include <thread>
#include <unistd.h>
#include <google/protobuf/util/time_util.h>
#include <grpc++/grpc++.h>

#include "fb.grpc.pb.h"

using google::protobuf::Timestamp;
using google::protobuf::Duration;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::ClientContext;
using grpc::Status;
using grpc::Channel;
using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;
using hw4::Message;
using hw4::ListReply;
using hw4::WorkerAddress;
using hw4::Request;
using hw4::Reply;
using hw4::AssignedWorkers;
using hw4::MessengerMaster;
using hw4::MessengerWorker;

using namespace std;

//Worker struct
class WorkerProcess {
    public:
    string hostname;
    string portnumber;
    unique_ptr<MessengerWorker::Stub> workerStub;
    
    WorkerProcess(){
        hostname = "";
        portnumber= "";
        workerStub = NULL;
    }
    
    WorkerProcess(string h, string p, shared_ptr<Channel> c){
        hostname = h;
        portnumber = p;
        workerStub = MessengerWorker::NewStub(c);
    }
};

//Client struct that holds a user's username, followers, and users they follow
struct Client {
    string username;
   
    //int following_file_size = 0;
    
    // usernames for the clients the user follows
    vector<string> clientFollowers;
    
    // usernames for the clients who follow the user
    vector<string> clientFollowing;
    
    // pointer to primary worker assigned to client
    WorkerProcess* primaryWorker;
    
    bool operator==(const Client& c1) const{
        return (username == c1.username);
    }
};

//Vector that stores every client that has been created
vector<Client> client_db;

//Helper function used to find a Client object given its username
int find_user(string username){
    int index = 0;
    for(Client c : client_db){
        if(c.username == username){
            return index;
        }
        index++;
    }
    return -1;
}

class MasterHelper {
    public:
    string masterAddress;
    //how do I create a new worker stub to push into this thingy
    vector<WorkerProcess*> listWorkers;
    
    MasterHelper(){
        masterAddress = "";
    }
    
    MasterHelper(string a){
        masterAddress = a;
    }
};

MasterHelper masterInfo;

//hostname and portnumber for the master
string master_hostname;
string master_portnumber;

// Logic and data behind the server's behavior.
class MessengerServiceMaster final : public MessengerMaster::Service {
  
   Status WorkerConnected(ServerContext* context, const WorkerAddress* request, Reply* reply) override {
       cout << "Master - New Worker Connected to Master\n";
       
       string hostname = request->host();
       string portnumber = request->port();
       
       shared_ptr<Channel> workerChannel = grpc::CreateChannel(hostname + ":" + portnumber, grpc::InsecureChannelCredentials());
       WorkerProcess* worker = new WorkerProcess(hostname, portnumber, workerChannel);
       
       //push worker object onto our list of workers
       masterInfo.listWorkers.push_back(worker);
       
       return Status::OK;
   }

    Status FindPrimaryWorker(ServerContext* context, const Request* request, AssignedWorkers* reply) override {
        /*
            HOW TO USE ASSIGNED WORKERS
            
            reply->set_primary(ADDRESS OF PRIMARY WORKER)
            reply->set_secondary1(ADDRESS OF SECONDARY WORKER 1)
            reply->set_secondary2(ADDRESS OF SECONDARY WORKER 2)
        */
        
        cout << "Master - Find primary and secondary workers for new client\n";
       
        int indexPrimary = -1;
        int indexSecondary1 = -1;
        int indexSecondary2 = -1;
        
        int currentMin = 999999;
        
        //initial loop to find the index for the primary worker
        for(int i = 0; i < masterInfo.listWorkers.size(); i++){
            
            ClientContext clientContext;
            Request clientRequest;
            Reply clientReply;
            
            Status status = masterInfo.listWorkers[i]->workerStub->NumberClientsConnected(&clientContext, clientRequest, &clientReply);
      
            if(status.ok()) {
                cout << clientReply.msg() << endl;
                if(stoi(clientReply.msg()) < currentMin){
                    indexPrimary = i;
                }
            }
            else {
                cout << status.error_code() << ": " << status.error_message()
                    << endl;
                cout << "Master - RPC failed\n";
            }
        }
        
        /* AS WE ONLY HAVE A SINGLE SERVER WORKING RIGHT NOW, THIS CODE IS NOT NECESSARY
        
        currentMin = 999999;
        
        //loop to find the index for the first secondary worker
        for(int i = 0; i < masterInfo.listWorkers.size(); i++){
            
            ClientContext clientContext;
            Request clientRequest;
            Reply clientReply;
            
            Status status = masterInfo.listWorkers[i]->workerStub->NumberClientsConnected(&clientContext, clientRequest, &clientReply);
      
            if(status.ok()) {
                cout << clientReply.msg() << endl;
                if(stoi(clientReply.msg()) < currentMin && masterInfo.listWorkers[i].hostname != masterInfo.listWorkers[indexPrimary].hostname){
                    indexSecondary1 = i;
                }
            }
            else {
                cout << status.error_code() << ": " << status.error_message()
                    << endl;
                cout << "RPC failed\n";
            }
        }
        
        currentMin = 999999;
        
        //loop to find the index for the second secondary worker
        for(int i = 0; i < masterInfo.listWorkers.size(); i++){
            
            ClientContext clientContext;
            Request clientRequest;
            Reply clientReply;
            
            Status status = masterInfo.listWorkers[i]->workerStub->NumberClientsConnected(&clientContext, clientRequest, &clientReply);
      
            if(status.ok()) {
                cout << clientReply.msg() << endl;
                if(stoi(clientReply.msg()) < currentMin && (masterInfo.listWorkers[i].hostname != masterInfo.listWorkers[indexPrimary].hostname || masterInfo.listWorkers[i].hostname != masterInfo.listWorkers[indexSecondary1].hostname)){
                    indexSecondary2 = i;
                }
            }
            else {
                cout << status.error_code() << ": " << status.error_message()
                    << endl;
                cout << "RPC failed\n";
            }
        }
        
        */
                   
        string primaryAddress = masterInfo.listWorkers[indexPrimary]->hostname + ":" + masterInfo.listWorkers[indexPrimary]->portnumber;
        reply->set_primary(primaryAddress);
        
        /*
                   
        string secondary1Address = masterInfo.listWorkers[indexSecondary1].hostname + ":" + masterInfo.listWorkers[indexSecondary1].portnumber;
        reply->set_secondary1(secondary1Address);
                   
        if(indexSecondary2 != -1){
            string secondary2Address = masterInfo.listWorkers[indexSecondary2].hostname + ":" + masterInfo.listWorkers[indexSecondary2].portnumber;
            reply->set_secondary2(secondary2Address);
        } else {
            //INVALID
            reply->set_secondary2("NONE");
        }
        
        */
        
        return Status::OK;
   }
};

void* RunMaster(void* v) {
    string address = master_hostname + ":" + master_portnumber;
    MessengerServiceMaster service;

    ServerBuilder builder;
    // Listen on the given address without any authentication mechanism.
    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    
    // Register "service" as the instance through which we'll communicate with
    // clients. In this case it corresponds to an *synchronous* service.
    builder.RegisterService(&service);
    
    // Finally assemble the server.
    unique_ptr<Server> master(builder.BuildAndStart());
    cout << "Master - Master listening on " << address << endl;

    //setting up MasterHelper class
    masterInfo = MasterHelper(address);

    // Wait for the server to shutdown. Note that some other thread must be
    // responsible for shutting down the server for this call to ever return.
    
    master->Wait();
    //thread masterThread(threadWait, master);
}

int main(int argc, char** argv) {
    
    //default
    master_portnumber = "4632";
    
    int opt = 0;
    while ((opt = getopt(argc, argv, "h:p:")) != -1){
        switch(opt) {
            case 'h':
                master_hostname = optarg;
                break;
            case 'p':
                master_portnumber = optarg;
                break;
            default:
                cerr << "Invalid Command Line Argument\n";
        }
    }
    
    pthread_t masterThread;
	pthread_create(&masterThread, NULL, RunMaster, NULL);
    
    cout << "Master - Thread started\n";
    
    while(true) {
        continue;
    }
    
    cout << "Master - Master is Shutting Down\n";
    
    return 0;
}
