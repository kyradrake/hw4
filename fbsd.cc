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
#include <pthread.h>
#include <string>
#include <stdlib.h>
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
using grpc::Status;
using hw4::Message;
using hw4::ListReply;
using hw4::Request;
using hw4::Reply;
using hw4::MessengerServer;

using namespace std;

//Client struct that holds a user's username, followers, and users they follow
struct Client {
    string username;
    bool connected = true;
    int following_file_size = 0;
    vector<Client*> client_followers;
    vector<Client*> client_following;
    ServerReaderWriter<Message, Message>* stream = 0;
    bool operator==(const Client& c1) const{
        return (username == c1.username);
    }
};

//Worker struct that maintains all the workers on a server
struct Worker {
    string worker_address;
    int clients_connected;
};

//Vector that stores every client that has been created
vector<Client> client_db;
vector<Worker> worker_db;

string master_address = "0.0.0.0:4633";

//Boolean to determine if the server is the master
//default is set to true
bool isMaster = true;

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

class MessengerServiceServer final : public MessengerServer::Service {
    
    // Master requests the server to start a new worker process
    // Server creates a new worker process and sends the data back to the master
    // master process -> server process
    Status CreateWorker(ServerContext* context, const Request* request, Reply* reply) override {
        // TO DO --------------------------------------------------------
        cout << "Creating a Worker\n";
        
        return Status::OK; 
    }

    // "Heartbeat" - make sure all workers are active
    // Master requests the server to periodically check all master processes
    // Server returns to master the address of any nonresponsive workers
    // master process -> server process
    Status CheckWorkers(ServerContext* context, const Request* request, Reply* reply) override {
        // TO DO --------------------------------------------------------
        cout << "Checking all Workers\n";
        
        return Status::OK; 
    }
    
    // Returns the primary worker - the worker with least clients connected
    // Master requests the server to return the address of the primary worker
    // master process -> server process
    Status FindPrimaryWorker(ServerContext* context, const Request* request, Reply* reply) override {
        // TO DO --------------------------------------------------------
        cout << "Finding Primary Worker\n";
        
        return Status::OK; 
    }

    // Connects the client to the specified server
    // Client initally connects to a known server address
    // Server replies with address of the master process
    // client process -> server process
    Status Connect(ServerContext* context, const Request* request, Reply* reply) override {
        cout << "Client Connecting\n";
        if (isMaster) {
            cout << "Redirecting client to " << master_address << endl;
            reply->set_msg(master_address);
        }
        else {
            // TO DO -------------------------------------------------------- 
            reply->set_msg("I'm not master, fix later");
        }
        return Status::OK; 
    }
    
    Status SaveMasterAddress(ServerContext* context, const Request* request, Reply* reply) override {
        if(request->username() != "Master") {
            reply->set_msg("Invalid Request - Must be Sent From a Master Process");
            return Status::OK;
        }
        
        master_address = request->arguments(0);
        if (isMaster) {
            reply->set_msg("Saved the master_address on the server holding the master");
        } else {
            reply->set_msg("Error: Saved the master_address on the server NOT holding the master");
        }
        return Status::OK; 
    }

};

void* RunServer(void* port_no) {
    string port = *(static_cast<std::string*>(port_no));
    string server_address = "0.0.0.0:"+port;
    MessengerServiceServer service;

    ServerBuilder builder;
    // Listen on the given address without any authentication mechanism.
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    
    // Register "service" as the instance through which we'll communicate with
    // clients. In this case it corresponds to an *synchronous* service.
    builder.RegisterService(&service);
    
    // Finally assemble the server.
    unique_ptr<Server> server(builder.BuildAndStart());
    
    cout << "Server listening on " << server_address << endl;
    
    
    // DELETE LATER
    isMaster = true;
    
    // if server is master, start master process
    // start 2 master replica processes, and 1 worker process
    if (isMaster) {
        // start master process
        
        /*
        master_address = "0.0.0.0:4633";
        string exec_master = "./master " + master_address;
        system(exec_master.c_str());
        cout << "In Server - Master listening on " << master_address << endl;
        */
        /*
        // start 1 worker process
        string worker_address = "0.0.0.0:3057";
        
        Worker w;
        w.worker_address = worker_address;
        w.clients_connected = 0;
        
        cout << "Here 1\n";
        
        string exec_worker = "./worker " + worker_address;
        system(exec_worker.c_str());
        cout << "In Server - Worker listening on " << worker_address << endl;
        */
        
        
        
        // TO DO ------------------------------------------------------
        // get the actual master address
        master_address = "0.0.0.0:4633";
        
        cout << "I am the Master Server\n";
        cout << "The Master Process's Address is: " << master_address << endl;
        
        cout << "\n";
        
    }
    else {
        // TO DO -------------------------------------------------------- 
        // find master address -- COULD WE HARDCODE THIS AND ALWAYS HAVE THE MASTER ON A CERTAIN PORT?
        master_address = "do this later";
        cout << "not master process, doing something else\n";
        
        //start 3 worker processes
        
        int initial_port = 3057;
        int num_workers = 0;
        while(num_workers < 3){
            string worker_address = "0.0.0.0:" + initial_port;
        
            Worker w;
            w.worker_address = worker_address;
            w.clients_connected = 0;

            string exec_worker = "./worker " + worker_address;
            
            //check to see if port number is available
            if(0 <= system(exec_worker.c_str())){
                cout << "Worker listening on " << worker_address << endl;
                num_workers++;
            }
            initial_port++;
        }
    }
    
    cout << "\n";

    // Wait for the server to shutdown. Note that some other thread must be
    // responsible for shutting down the server for this call to ever return.
    server->Wait();
    
    return 0;
}

int main(int argc, char** argv) {
  
    string port = "4632";
    int opt = 0;
    while ((opt = getopt(argc, argv, "p:")) != -1){
        switch(opt) {
            case 'p':
                port = optarg;
                break;
            case 'm':
                isMaster = optarg;
                break;
            default:
                cerr << "Invalid Command Line Argument\n";
        }
    }
    
    // start server service on given port number
    pthread_t pthread;
    
    pthread_create(&pthread, NULL, &RunServer, static_cast<void*>(new string(port)));
    //RunServer(static_cast<void*>(new string(port)));
    
    while(true) {
        continue;
    }
    
    cout << "Server is Shutting Down\n";
    

    return 0;
}
