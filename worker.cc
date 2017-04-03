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

class MessengerServiceImpl final : public MessengerServer::Service {

    Status Chat(ServerContext* context, ServerReaderWriter<Message, Message>* stream) override {
        Message message;
        Client *c;
        //Read messages until the client disconnects
        while(stream->Read(&message)) {
            string username = message.username();
            int user_index = find_user(username);
            c = &client_db[user_index];
            
            //Write the current message to "username.txt"
            string filename = username+".txt";
            ofstream user_file(filename,ios::app|ios::out|ios::in);
            google::protobuf::Timestamp temptime = message.timestamp();
            string time = google::protobuf::util::TimeUtil::ToString(temptime);
            string fileinput = time+" :: "+message.username()+":"+message.msg()+"\n";
            
            //"Set Stream" is the default message from the client to initialize the stream
            if(message.msg() != "Set Stream")
                user_file << fileinput;
            
            //If message = "Set Stream", print the first 20 chats from the people you follow
            else{
                if(c->stream==0)
                    c->stream = stream;
                string line;
                vector<string> newest_twenty;
                ifstream in(username+"following.txt");
                int count = 0;
                
                //Read the last up-to-20 lines (newest 20 messages) from userfollowing.txt
                while(getline(in, line)){
                    if(c->following_file_size > 20){
                        if(count < c->following_file_size-20){
                            count++;
                            continue;
                        }
                    }
                    newest_twenty.push_back(line);
                }
                
                Message new_msg; 
                //Send the newest messages to the client to be displayed
                for(int i = 0; i<newest_twenty.size(); i++){
                    new_msg.set_msg(newest_twenty[i]);
                    stream->Write(new_msg);
                }    
                continue;
            }
            
            //Send the message to each follower's stream
            vector<Client*>::const_iterator it;
            for(it = c->client_followers.begin(); it!=c->client_followers.end(); it++){
                Client *temp_client = *it;
                if(temp_client->stream!=0 && temp_client->connected)
                    temp_client->stream->Write(message);
                
                //For each of the current user's followers, put the message in their following.txt file
                string temp_username = temp_client->username;
                string temp_file = temp_username + "following.txt";
                ofstream following_file(temp_file,ios::app|ios::out|ios::in);
                following_file << fileinput;
                temp_client->following_file_size++;
                ofstream user_file(temp_username + ".txt",ios::app|ios::out|ios::in);
                user_file << fileinput;
            }
        }
        
        //If the client disconnected from Chat Mode, set connected to false
        c->connected = false;
        return Status::OK;
    }
    
    Status Worker(ServerContext* context, ServerReaderWriter<Message, Message>* stream) override {
        workerPort = reply.msg();
        //figure out how to actually send the chat messages simultaneously with the chat function
        return Status::OK; 
    }
    
};

void RunWorker(string port_no) {
    string worker_address = "0.0.0.0:"+port_no;
    MessengerServiceImpl service;

    ServerBuilder builder;
    // Listen on the given address without any authentication mechanism.
    builder.AddListeningPort(worker_address, grpc::InsecureServerCredentials());
    
    // Register "service" as the instance through which we'll communicate with
    // clients. In this case it corresponds to an *synchronous* service.
    builder.RegisterService(&service);
    
    // Finally assemble the server.
    unique_ptr<Server> worker(builder.BuildAndStart());
    cout << "Server listening on " << worker_address << endl;

    // Wait for the server to shutdown. Note that some other thread must be
    // responsible for shutting down the server for this call to ever return.
    worker->Wait();
}

int main(int argc, char** argv) {
  
    string port = "3055";
    int opt = 0;
    while ((opt = getopt(argc, argv, "p:")) != -1){
        switch(opt) {
            case 'p':
                port = optarg;
                break;
            default:
                cerr << "Invalid Command Line Argument\n";
        }
    }
    RunWorker(port);

    return 0;
}