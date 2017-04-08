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
using hw4::MessengerWorker;

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

//Vector that stores every client that has been created
vector<Client> client_db;

string worker_address = "";

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

class MessengerServiceWorker final : public MessengerWorker::Service {

    //Sends the list of total rooms and joined rooms to the client
    Status List(ServerContext* context, const Request* request, ListReply* list_reply) override {
        Client user = client_db[find_user(request->username())];
        int index = 0;
        
        for(Client c : client_db){
            list_reply->add_all_rooms(c.username);
        }
        
        vector<Client*>::const_iterator it;
        
        for(it = user.client_following.begin(); it!=user.client_following.end(); it++){
            list_reply->add_joined_rooms((*it)->username);
        }
        
        return Status::OK;
    }

    //Sets user1 as following user2
    Status Join(ServerContext* context, const Request* request, Reply* reply) override {
        string username1 = request->username();
        string username2 = request->arguments(0);
        int join_index = find_user(username2);
        
        //If you try to join a non-existent client or yourself, send failure message
        if(join_index < 0 || username1 == username2) {
            reply->set_msg("Join Failed -- Invalid Username");
        }
        else {
            Client *user1 = &client_db[find_user(username1)];
            Client *user2 = &client_db[join_index];
            
            //If user1 is following user2, send failure message
            if(find(user1->client_following.begin(), user1->client_following.end(), user2) != user1->client_following.end()){
                reply->set_msg("Join Failed -- Already Following User");
                return Status::OK;
            }
            user1->client_following.push_back(user2);
            user2->client_followers.push_back(user1);
            reply->set_msg("Join Successful");
        }
        return Status::OK; 
    }

    //Sets user1 as no longer following user2
    Status Leave(ServerContext* context, const Request* request, Reply* reply) override {
        string username1 = request->username();
        string username2 = request->arguments(0);
        int leave_index = find_user(username2);
        
        //If you try to leave a non-existent client or yourself, send failure message
        if(leave_index < 0 || username1 == username2){
            reply->set_msg("Leave Failed -- Invalid Username");
        }
        else{
            Client *user1 = &client_db[find_user(username1)];
            Client *user2 = &client_db[leave_index];
            
            //If user1 isn't following user2, send failure message
            if(find(user1->client_following.begin(), user1->client_following.end(), user2) == user1->client_following.end()){
                reply->set_msg("Leave Failed -- Not Following User");
                return Status::OK;
            }
            
            // find the user2 in user1 following and remove
            user1->client_following.erase(find(user1->client_following.begin(), user1->client_following.end(), user2)); 
            
            // find the user1 in user2 followers and remove
            user2->client_followers.erase(find(user2->client_followers.begin(), user2->client_followers.end(), user1));
            reply->set_msg("Leave Successful");
        }
        return Status::OK;
    }

    //Called when the client startd and checks whether their username is taken or not
    Status Login(ServerContext* context, const Request* request, Reply* reply) override {
        cout << "Client is logging in\n";
        Client c;
        string username = request->username();
        int user_index = find_user(username);
        if(user_index < 0){
            c.username = username;
            client_db.push_back(c);
            reply->set_msg("Login Successful!");
            cout << "Successful Login\n";
        }
        else{ 
            Client *user = &client_db[user_index];
            if(user->connected) {
                cout << "Unsuccessful Login\n";
                reply->set_msg("Invalid Username");
            }
            else{
                cout << "Successful Login\n";
                string msg = "Welcome Back " + user->username;
                reply->set_msg(msg);
                user->connected = true;
            }
        }
        return Status::OK;
    }
    
    // Connects the client to the specified server
    // Client initally connects to a known server address
    // Server replies with address of the master process
    // client process -> server process
    Status Connect(ServerContext* context, const Request* request, Reply* reply) override {
        cout << "Client Connecting\n";
        
        /*
        if (isMaster) {
            cout << "Redirecting client to " << master_address << endl;
            reply->set_msg(master_address);
        }
        else {
            // TO DO -------------------------------------------------------- 
            reply->set_msg("I'm not master, fix later");
        }
        
        */
        
        reply->set_msg(worker_address);
        
        return Status::OK; 
        
        
        
        
        /* 
            TO DO ----------------------------------------------------------------------
            Should call FindPrimaryWorker on each of the servers and then determine Primary, Secondary1, and Secondary2
            
            To do that the master needs to know each of the servers
            Add a RPC for a server to "find" master, and create a MessengerServer service for each of them
            
            A temporary solution would be to hardcode the server address/port on local host 
            Or possibly hardcode the worker address/port 
            Forget about secondary workers for now
        
        */
        /*
        int min_clients_connected = 999999;
        string worker_address = "";
        
        if (worker_db.size() == 0) {
            cout << "Error: No workers in the worker database." << endl;
            return Status::OK;
        }
        
        for (Worker w: worker_db) {
            if (w.numClientsConnected < min_clients_connected) {
                worker_address = w.hostname + ":" + w.portnumber; //I think this is the correct syntax
                min_clients_connected = w.numClientsConnected;
            }
        }
        
        reply->set_msg(worker_address);
        return Status::OK; */
    }
    
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
        //workerPort = reply.msg();
        //figure out how to actually send the chat messages simultaneously with the chat function
        return Status::OK; 
    }
    
    
    Status UpdateMasterAddress(ServerContext* context, const Request* request, Reply* reply) override {
        cout << "Updating master address\n";
        
        return Status::OK;
    }
    
    Status NumberClientsConnected(ServerContext* context, const Request* request, Reply* reply) override {
        cout << "Tell master how many clients are connected \n";
        
        return Status::OK;
    }
    
};

//I think this is how we want to connect from a worker to a master. If not, I'm still very confused.

/*

class MessengerWorker {
    public:
    
    MessengerWorker(shared_ptr<Channel> channel, string uname){
        serverStub = MessengerServer::NewStub(channel);
        username = uname;
    }
    
    // Sends request from worker to server to initialize worker struct on the master
    void WorkerInitialize(string workerAddress) {

        if(v  == NULL){
            cout << "Server has not been initialized." << endl;
            return;
        }

        WorkerRequest request;

        request.set_address(workerAddress);

        Reply reply;

        ClientContext context;

        Status status = serverStub->WorkerInitialize(&context, request, &reply);

        if(status.ok()) {
            cout << reply.msg() << endl;
        }
        else {
            cout << status.error_code() << ": " << status.error_message() << endl;
            cout << "RPC failed\n";
        }
    }
    
    private:
    string username;
    unique_ptr<MessengerServer::Stub> serverStub;
};

*/

void RunWorker(string port) {
    worker_address = "0.0.0.0:"+port;
    MessengerServiceWorker service;

    ServerBuilder builder;
    // Listen on the given address without any authentication mechanism.
    builder.AddListeningPort(worker_address, grpc::InsecureServerCredentials());
    
    // Register "service" as the instance through which we'll communicate with
    // clients. In this case it corresponds to an *synchronous* service.
    builder.RegisterService(&service);
    
    // Finally assemble the server.
    unique_ptr<Server> worker(builder.BuildAndStart());
    cout << "Worker listening on " << worker_address << endl;

    // Wait for the server to shutdown. Note that some other thread must be
    // responsible for shutting down the server for this call to ever return.
    worker->Wait();
}

int main(int argc, char** argv) {
    RunWorker(argv[1]);
    
    cout << "Worker shutting down\n";
    return 0;
}