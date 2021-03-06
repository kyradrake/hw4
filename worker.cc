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
#include <queue>
#include <string>
#include <stdlib.h>
#include <vector>
#include <unistd.h>
#include <google/protobuf/util/time_util.h>
#include <grpc++/grpc++.h>

#include "fb.grpc.pb.h"

using google::protobuf::Timestamp;
using google::protobuf::Duration;
using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;
using hw4::Message;
using hw4::ClientListReply;
using hw4::Request;
using hw4::Reply;
using hw4::WorkerAddress;
using hw4::FollowerMessage;
using hw4::AssignedWorkers;
using hw4::SaveMessage;
using hw4::MessengerWorker;
using hw4::MessengerMaster;
using hw4::CreateWorkerRequest;
using hw4::ListReply;

using namespace std;

class Client;

class WorkerToMasterConnection;
class WorkerToWorkerConnection;

WorkerToWorkerConnection* findWorker(string wAddress);

// connection to master
WorkerToMasterConnection* masterConnection;

// connections to other workers
vector<WorkerToWorkerConnection*> workerConnections;

// clients connected to worker
vector<Client> clientsConnected = vector<Client>();

// worker's own address
string workerHost = "";
string workerPort = "";
string workerAddress = "";

// master's address
string masterAddress = "";

// vector clock
int clockIndex;
vector<int> vectorClock = vector<int>();

vector<int> UpdateVectorClock(vector<int> v) {
    vector<int> clock = vector<int>();
    if(v.size() != vectorClock.size()) {
        //cout << "Error - VectorClock can't Update\n";
        //cout << "v.size() = " << v.size() << endl;
        //cout << "vectorClock.size() = " << vectorClock.size() << endl;
        return vectorClock;
    }
    for(int i=0; i<v.size(); i++) {
        int localTime = vectorClock[i];
        int newTime = v[i];
        
        // if index for this worker, increment time by 1
        if(i == clockIndex) {
            clock.push_back(localTime + 1);
        }
        else if(localTime > newTime) {
            clock.push_back(localTime);
        }
        else {
            clock.push_back(newTime);
        }
    }
    return clock;
}

string printVectorClock() {
    string s = "<";
    for(int i=0; i<(vectorClock.size()-1); i++) {
        s += vectorClock[i];
        s += ",";
    }
    s += vectorClock[vectorClock.size()-1];
    s += ">";
}

// struct to hold information about other clients
struct ClientFollower {
    string username;
    
    // address of worker assigned to this client
    string worker; 
    
    ClientFollower(string uname, string w) {
        username = uname;
        worker = w;
    }
    
    void updateWorker(string w) {
        worker = w;
    }
};


//Client struct that holds a user's username, followers, and users they follow
struct Client {
    string username;
    
    int following_file_size = 0;
    
    // usernames for the clients the user follows
    vector<ClientFollower> clientFollowers;
    
    // usernames for the clients who follow the user
    vector<string> clientFollowing;
    
    ServerReaderWriter<Message, Message>* stream = 0;
    
    // addresses for the client's primary and secondary workers
    string primaryWorker;
    string secondary1Worker;
    string secondary2Worker;
    
    bool operator==(const Client& c1) const{
        return (username == c1.username);
    }
};

int findUser(string username) {
    int index = 0;
    for (Client c : clientsConnected) {
        if (c.username == username)
            return index;
        index++;
    }
    return -1;
}

class WorkerToMasterConnection {
    public:
    unique_ptr<MessengerMaster::Stub> masterStub;
    
    WorkerToMasterConnection(shared_ptr<Channel> channel){
        masterStub = MessengerMaster::NewStub(channel);
    }
    
    // notifies master that a new worker process has begun
    // sends the workers host and port to master
    Status WorkerConnected(string workerHost, string workerPort, int clock) {
        // Data sent to master 
        WorkerAddress request;
        request.set_host(workerHost);
        request.set_port(workerPort);
        request.set_clock(clock);
        
        // Data received from master
        Reply reply;
        
        // Context for the client
        ClientContext context;
        
        Status status = masterStub->WorkerConnected(&context, request, &reply);
        
        if(status.ok()) {
            cout << "Worker - Worker Connected to Master Process" << endl;
            return Status::OK;
        }
        else {
            return Status::CANCELLED;
        }
    }
    
    // sends request to master to get the address for a client's primary worker
    string GetClientsPrimaryWorker(string username) {
        // Data sent to master
        Request request;
        request.set_username(username);
        
        // Container for the data from the master
        Reply reply;
        
        // Context for the worker
        ClientContext context;
        
        Status status = masterStub->GetClientsPrimaryWorker(&context, request, &reply);
        
        if(status.ok()) {
            string clientsPrimaryWorker = reply.msg();
            return clientsPrimaryWorker;
        }
        else {
            cout << "ERROR - GetClientsPrimaryWorker Failed\n";
            return "";
        }
    }
    
    // returns a vector of addresses to the primary and secondary workers assigned to the specified client
    vector<string> FindPrimaryWorker(string clientUsername) {
        if(masterStub == NULL) {
            vector<string> error;
            error.push_back("NULL ERROR");
            return error;
        }
        
        // Data being sent to the server
        Request request;
        request.set_username(clientUsername);
        
        // Container for the data from the server
        AssignedWorkers reply;
        
        // Context for the client
        ClientContext context;
        
        Status status = masterStub->FindPrimaryWorker(&context, request, &reply);
        
        if(status.ok()) {
            cout << "Worker - Primary Worker: " << reply.primary() << endl;
            
            vector<string> workers;
            
            workers.push_back(reply.primary());
            workers.push_back(reply.secondary1());
            workers.push_back(reply.secondary2());
            
            return workers;
        }
        else {
            cout << "Worker - Error: " << status.error_code() << ": " << status.error_message() << endl;
            
            vector<string> workers;
            workers.push_back("ERROR");
            
            return workers;
        }
    }
    
    // call this when we need to update clientsConnected with the database from the master
    void UpdateClientData(string username) {
        
        // Data sent to master
        Request request;
        request.set_username(username);
        
        // Container for the data from the master
        ClientListReply reply;
        
        // Context for the worker
        ClientContext context;
        
        Status status = masterStub->UpdateClientData(&context, request, &reply);
        
        if(status.ok()) {
            
            Client client;
            
            // set client username
            client.username = username;
            
            // add in followers
            for(int i = 0; i < reply.followers().size(); i++){
                // get follower's username
                string fUsername = reply.followers(i);
                
                // get follower's primary worker
                string fWorkerAddress = masterConnection->GetClientsPrimaryWorker(fUsername);
                
                ClientFollower follower(fUsername, fWorkerAddress);
                
                client.clientFollowers.push_back(follower);
            }
            
            // add in following
            for(int i = 0; i < reply.following().size(); i++){
                client.clientFollowing.push_back(reply.following(i));
            }
            
            //check to see if updating, or creating new data
            bool alreadyExists = false;
            for(int i = 0; i < clientsConnected.size(); i++){
                if(clientsConnected[i].username == username) {
                    client.primaryWorker = clientsConnected[i].primaryWorker;
                    client.secondary1Worker = clientsConnected[i].secondary1Worker;
                    client.secondary2Worker = clientsConnected[i].secondary2Worker;
                    
                    client.stream = clientsConnected[i].stream;
                    
                    clientsConnected[i] = client;
                    alreadyExists = true;
                }
            }
            if(!alreadyExists){
                clientsConnected.push_back(client);
            }
        }
        else {
            cout << "SOMETHING BAD HAPPENED IN UPDATECLIENTWORKER" << endl;
        }
    }
    
    string GetWorkerOnHost(string host) {
        // Data being sent to the server
        Request request;
        request.add_arguments(host);
        
        // Container for the data from the server
        Reply reply;
        
        // Context for the client
        ClientContext context;
        
        // Get a worker on host
        Status status = masterStub->GetWorkerOnHost(&context, request, &reply);
        
        if(status.ok()) {
            // Reply = "Failure" if server is dead
            return reply.msg();
        }
        else {
            return "Failure";
        }
    }
};

class WorkerToWorkerConnection {
    public:
    string connectedWorkerAddress;
    unique_ptr<MessengerWorker::Stub> workerStub;
    
    WorkerToWorkerConnection(string waddress, shared_ptr<Channel> channel) {
        connectedWorkerAddress = waddress;
        workerStub = MessengerWorker::NewStub(channel);
    }
    
    string getHostname(){
        return connectedWorkerAddress.substr(0,24);
    }
    
    // sends the message to the specified user's worker
    void SendMessageToFollower(string followerUsername, string msg, vector<int> clock) {
        // Data being sent to the follower's worker
        FollowerMessage request;
        request.set_username(followerUsername);
        request.set_msg(msg);
        
        for(int i : clock) {
            request.add_clock(i);
        }
        
        // Container for the data from the follower's worker
        Reply reply;
        
        // Context for the client
        ClientContext context;
        
        Status status = workerStub->MessageForFollower(&context, request, &reply);
        
        if(!status.ok()) {
            cout << "Message to Follower Could not Send\n";
        }
    }
    
    // sends a message to the worker to save a chat message in the user's text file
    // and save the chat message to the user's followers' text files
    string SaveChat(string username, vector<string> followerUsernames, string chatMessage) {
        SaveMessage request;
        request.set_username(username);
        request.set_message(chatMessage);
        for(int i=0; i<followerUsernames.size(); i++) {
            request.add_followers(followerUsernames[i]);
        }
        
        Reply reply;
        ClientContext context;
        
        Status status = workerStub->SaveChat(&context, request, &reply);
        
        if(!status.ok()) {
            // SaveChat failed, find a new secondary worker
            cout << "ERROR: Didn't send chat to " << connectedWorkerAddress << endl;
            
            // ask master for new worker on same host
            return masterConnection->GetWorkerOnHost(getHostname());
        }
        return connectedWorkerAddress;
    }
};

// Searches for a connection to a worker with the specified address
// If it cannot be found in the database, establish a connection to the worker
WorkerToWorkerConnection* findWorker(string wAddress) {
    // first, look for worker in the database
    for(WorkerToWorkerConnection* w : workerConnections) {
        if(w->connectedWorkerAddress == wAddress) {
            return w;
        }
    }
    
    // if worker isn't found, create a connection to it in the database
    shared_ptr<Channel> channel = grpc::CreateChannel(wAddress, grpc::InsecureChannelCredentials());
    WorkerToWorkerConnection* w = new WorkerToWorkerConnection(wAddress, channel);
    workerConnections.push_back(w);
    return w;
}

class MessengerServiceWorker final : public MessengerWorker::Service {

    //Sends the list of total rooms and joined rooms to the client
    Status List(ServerContext* context, const Request* request, Reply* reply) override {
        
        //Data being sent to the server
        Request requestMaster;
  
        //Container for the data from the server
        Reply replyMaster;

        //Context for the client
        ClientContext contextClient;

        Status status = masterConnection->masterStub->ListMaster(&contextClient, requestMaster, &replyMaster);
        
        if(status.ok()) {
            string msgForward = replyMaster.msg();
            reply->set_msg(msgForward);
        } else {
            cout << "SOMETHING BAD HAPPENED IN LIST" << endl;
        }
        return Status::OK;
    }

    //Sets user1 as following user2
    Status Join(ServerContext* context, const Request* request, Reply* reply) override {
        
        string username = request->username();
        string userToJoin = request->arguments(0);
        
        //Data being sent to the server
        Request requestMaster;
        requestMaster.set_username(username);
        requestMaster.add_arguments(userToJoin);
  
        //Container for the data from the server
        Reply replyMaster;

        //Context for the client
        ClientContext contextClient;

        Status status = masterConnection->masterStub->JoinMaster(&contextClient, requestMaster, &replyMaster);
        
        if(status.ok()) {
            string msgForward = replyMaster.msg();
            reply->set_msg(msgForward);
        } else {
            cout << "SOMETHING BAD HAPPENED IN JOIN" << endl;
        }
        return Status::OK;
    }

    //Sets user1 as no longer following user2
    Status Leave(ServerContext* context, const Request* request, Reply* reply) override {
        
        string username = request->username();
        string userToLeave = request->arguments(0);
        
        //Data being sent to the server
        Request requestMaster;
        requestMaster.set_username(username);
        requestMaster.add_arguments(userToLeave);
  
        //Container for the data from the server
        Reply replyMaster;

        //Context for the client
        ClientContext contextClient;

        Status status = masterConnection->masterStub->LeaveMaster(&contextClient, requestMaster, &replyMaster);
        
        if(status.ok()) {
            string msgForward = replyMaster.msg();
            reply->set_msg(msgForward);
        } else {
            cout << "SOMETHING BAD HAPPENED IN LEAVE" << endl;
        }
        return Status::OK;
    }

    //Called when the client startd and checks whether their username is taken or not
    Status Login(ServerContext* context, const Request* request, Reply* reply) override {   
        string username = request->username();
        string primaryAddress = request->arguments(0);
        string secondary1Address = request->arguments(1);
        string secondary2Address = request->arguments(2);
        
        int clientIndex = findUser(username);
        
        // Add User to the Database if it hasn't been added yet
        if(clientIndex == -1) {
            Client c;
            c.username = username;
            c.primaryWorker = primaryAddress;
            c.secondary1Worker = secondary1Address;
            c.secondary2Worker = secondary2Address;
            clientsConnected.push_back(c);
        }
        else {
            clientsConnected[clientIndex].primaryWorker = primaryAddress;
            clientsConnected[clientIndex].secondary1Worker = secondary1Address;
            clientsConnected[clientIndex].secondary2Worker = secondary2Address;
        }
        
        //Data being sent to the server
        Request requestMaster;
        requestMaster.set_username(username);
        requestMaster.add_arguments(primaryAddress);
        requestMaster.add_arguments(secondary1Address);
        requestMaster.add_arguments(secondary2Address);
  
        //Container for the data from the server
        Reply replyMaster;

        //Context for the client
        ClientContext contextClient;

        Status status = masterConnection->masterStub->LoginMaster(&contextClient, requestMaster, &replyMaster);
        
        if(status.ok()) {
            string msgForward = replyMaster.msg();
            reply->set_msg(msgForward);
        } else {
            cout << "SOMETHING BAD HAPPENED IN LOGIN" << endl;
        }
        
        //update the data in our client database
        masterConnection->UpdateClientData(username);
        
        return Status::OK;
    }
    
    // Connects the client to the specified server
    // Client initally connects to a known server address
    // Server replies with address of the master process
    // client process -> server process
    Status Connect(ServerContext* context, const Request* request, AssignedWorkers* reply) override {
        cout << "Client Connecting\n";
        
        string clientUsername = request->username();
        
        vector<string> assignedWorkers = masterConnection->FindPrimaryWorker(clientUsername);
        
        if(assignedWorkers.size() != 3) {
            return Status::CANCELLED;
        }
        
        reply->set_primary(assignedWorkers[0]);
        reply->set_secondary1(assignedWorkers[1]);
        reply->set_secondary2(assignedWorkers[2]);
        
        return Status::OK;
    }
    
    Status Chat(ServerContext* context, ServerReaderWriter<Message, Message>* stream) override {
        
        Message message;
        Client* c;
        // Read messages until the client disconnects
        while(stream->Read(&message)) {
            string username = message.username();
            
            int clientIndex = findUser(username);
            c = &clientsConnected[clientIndex];
            
            // initialize client stream if it hasn't been yet
            if(c->stream == 0) {
                c->stream = stream;
                //update the data in our client database
                masterConnection->UpdateClientData(username);
            }
            
            c->primaryWorker = message.primary();
            c->secondary1Worker = message.secondary1();
            c->secondary2Worker = message.secondary2();
            
            vector<int> clock = vector<int>();
            for(int i : message.clock()) {
                clock.push_back(i);
            }
            UpdateVectorClock(clock);
            
            // open "username.txt"
            string filename = username + ".txt";
            ofstream userFile(filename,ios::app|ios::out|ios::in);
            
            // generate message to output to file and to followers
            google::protobuf::Timestamp temptime = message.timestamp();
            string time = google::protobuf::util::TimeUtil::ToString(temptime);
            
            //string time = printVectorClock();
            string fileinput = time + " " + message.username() + ":" + message.msg() + "\n";
            
            // "Set Stream" is the default message from the client to initialize the stream
            if(message.msg() != "Set Stream") {
                // write message to "username.txt"
                userFile << fileinput;
                
                // store usernames for all followers
                vector<string> followerUsernames = vector<string>();
                
                // send message to each follower
                for(ClientFollower follower : c->clientFollowers) {
                    // send message to follower's chat stream
                    findWorker(follower.worker)->SendMessageToFollower(follower.username, fileinput, vectorClock);
                    
                    // get follower's username
                    string followerUsername = follower.username;
                    followerUsernames.push_back(followerUsername);

                    // open following.txt file on local server
                    string followingFile = followerUsername + "following.txt";
                    ofstream file(followingFile,ios::app|ios::out|ios::in);

                    // add new message to following.txt file
                    file << fileinput;
                }
                
                // write message on other two servers
                if(c->secondary1Worker != "NONE") {
                    string sec1Worker = findWorker(c->secondary1Worker)->SaveChat(username, followerUsernames, fileinput); 
                    
                    if(c->secondary1Worker != sec1Worker) {
                        c->secondary1Worker = sec1Worker;
                        findWorker(c->secondary1Worker)->SaveChat(username, followerUsernames, fileinput); 
                    }
                }
                if(c->secondary2Worker != "NONE") {
                    string sec2Worker = findWorker(c->secondary2Worker)->SaveChat(username, followerUsernames, fileinput);
                    
                    if(c->secondary2Worker != sec2Worker) {
                        c->secondary2Worker = sec2Worker;
                        findWorker(c->secondary2Worker)->SaveChat(username, followerUsernames, fileinput); 
                    }
                }
            }
            //If message = "Set Stream", print the first 20 chats from the people you follow
            else{
                string line;
                vector<string> newestTwenty;
                ifstream in(username+"following.txt");
                int count = 0;
                
                // Get size of following.txt file
                int followingFileSize = 0;
                ifstream i(username+"following.txt");
                while(getline(i,line)) {
                    followingFileSize++;
                }
                
                //Read the last up-to-20 lines (newest 20 messages) from userfollowing.txt
                while(getline(in, line)){
                    if(followingFileSize > 20){
                        if(count < followingFileSize-20){
                            count++;
                            continue;
                        }
                    }
                    newestTwenty.push_back(line);
                }
                
                Message newMsg; 
                //Send the newest messages to the client to be displayed
                for(int i = 0; i<newestTwenty.size(); i++){
                    newMsg.set_msg(newestTwenty[i]);
                    newMsg.set_primary(c->primaryWorker);
                    newMsg.set_secondary1(c->secondary1Worker);
                    newMsg.set_secondary2(c->secondary2Worker);
                    
                    stream->Write(newMsg);
                }    
                continue;
            } 
        }
        return Status::OK;
    }
    
    // Master sends RPC to worker to check how many clients are connected to the worker
    Status NumberClientsConnected(ServerContext* context, const Request* request, Reply* reply) override {
        
        reply->set_msg("" + to_string(clientsConnected.size()));
        
        return Status::OK;
    }
    
    // Worker sends RPC to worker to write a chat message to a client's stream
    Status MessageForFollower(ServerContext* context, const FollowerMessage* request, Reply* reply) override {
        string username = request->username();
        string message = request->msg();
        
        vector<int> newClock = vector<int>();
        for(int i : request->clock()) {
            newClock.push_back(i);
        }
        UpdateVectorClock(newClock);
        
        // Find client in database message is for
        int userIndex = findUser(username);
        
        // Make sure client is in the database
        // If so, this is the primary worker
        if(userIndex != -1) {
            Client* client = &clientsConnected[userIndex];
        
            // Write message to client's stream if they are in chat mode
            if(client->stream != 0) {
                Message newMsg; 
                newMsg.set_msg(message);
                newMsg.set_primary(client->primaryWorker);
                newMsg.set_secondary1(client->secondary1Worker);
                newMsg.set_secondary2(client->secondary2Worker);
                
                client->stream->Write(newMsg);
            }
        }
        else {
            reply->set_msg("Not Primary Worker");
            return Status::OK;
        }
        reply->set_msg("Success");
        
        return Status::OK;
    }
    
    // "Heartbeat" function - master checks to see if worker is still active
    Status CheckWorker(ServerContext* context, const Request* request, Reply* reply) override {
        reply->set_msg("lub-DUB");
        
        return Status::OK;
    }
    
    // Start a new worker on the server
    Status StartNewWorker(ServerContext* context, const CreateWorkerRequest* request, Reply* reply) override {
        
        string workerHostname = request->worker_hostname();
        string workerPort = request->worker_port();
        string masterHostname = request->master_hostname();
        string masterPort = request->master_port();
        
        pid_t child = fork();
        if(child == 0){
            char* argv[11];
            
            vector<string> args = {"./worker", "-h", workerHostname, "-p", workerPort, "-m", masterHostname, "-a", masterPort, "&"};
            
            for(int i = 0; i < args.size(); i++){
                string a = args[i];
                argv[i] = (char *)a.c_str();
            }
            
            argv[10] = NULL;
            
            execv("./worker", argv);
        }
        
        reply->set_msg("Made new worker on: " + workerPort);
        
        return Status::OK;
    }
    
    // save a chat message in the text files for the user who sent the message and all the followers of that user
    Status SaveChat(ServerContext* context, const SaveMessage* request, Reply* reply) override {
        string username = request->username();
        string chatMessage = request->message();
        
        //find the user in the database we need to write to
        int clientIndex = findUser(username);
        Client* c = &clientsConnected[clientIndex];
        
        //save the chat message in their text file
        string filename = username + ".txt";
        ofstream userFile(filename,ios::app|ios::out|ios::in);
        userFile << chatMessage;
        
        //save the chat message in their followers text files
        for(string follower : request->followers()) {
            string followingFile = follower + "following.txt";
            ofstream file(followingFile,ios::app|ios::out|ios::in);
            file << chatMessage;
        }
        
        return Status::OK;
    }
    
    // update the master's address to that of the master replica taking over
    Status UpdateMaster(ServerContext* context, const Request* request, Reply* reply) override {
        
        masterAddress = request->arguments(0) + ":" + request->arguments(1);
        
        cout << "masterAddress is now: " << masterAddress;
        
        shared_ptr<Channel> channel = grpc::CreateChannel(masterAddress, grpc::InsecureChannelCredentials());
        masterConnection = new WorkerToMasterConnection(channel);

        //keep on re-running until we connect to the master
        bool connected = false;
        while(!connected){
            Status status = masterConnection->WorkerConnected(workerHost, workerPort, clockIndex);
            if(status.ok()){
                connected = true;
            }
        }
        
        cout << "Started connection to updated master on address: " << masterAddress;
        
        return Status::OK;
    }
    
};

void* RunWorker(void* v) {
    MessengerServiceWorker service;

    ServerBuilder builder;
    // Listen on the given address without any authentication mechanism.
    builder.AddListeningPort(workerAddress, grpc::InsecureServerCredentials());
    
    // Register "service" as the instance through which we'll communicate with
    // clients. In this case it corresponds to an *synchronous* service.
    builder.RegisterService(&service);
    
    // Finally assemble the server.
    unique_ptr<Server> worker(builder.BuildAndStart());
    cout << "Worker - Worker listening on " << workerAddress << endl;

    // Wait for the server to shutdown. Note that some other thread must be
    // responsible for shutting down the server for this call to ever return.
    worker->Wait();
}

void ConnectToMaster(string workerHost, string workerPort) {
    shared_ptr<Channel> channel = grpc::CreateChannel(masterAddress, grpc::InsecureChannelCredentials());
    masterConnection = new WorkerToMasterConnection(channel);
    
    //keep on re-running until we connect to the master
    bool connected = false;
    while(!connected){
        Status status = masterConnection->WorkerConnected(workerHost, workerPort, clockIndex);
        if(status.ok()){
            connected = true;
        }
    }
}

void AskForFile(string nameOfFile){
    
    Request request;
    request.add_arguments(nameOfFile);
    
    ListReply reply;
    ClientContext context;

    Status status = masterConnection->masterStub->AskForFile(&context, request, &reply);
    
    if(status.ok()) {
        string filename = nameOfFile + ".txt";
        ofstream file(filename,ios::app|ios::out|ios::in);
        for(int i = 0; i < reply.msgs().size(); i++) {
            file << reply.msgs(i);
        }
    } else {
        cout << "SOMETHING BAD HAPPENED IN ASKFORFILES" << endl;
    }
}

vector<string> GetAllClients(){
    
    Request request;
    ListReply reply;
    ClientContext context;

    Status status = masterConnection->masterStub->GetAllClients(&context, request, &reply);

    vector<string> rVal;
    
    if(status.ok()) {
        for(int i = 0; i < reply.msgs().size(); i++){
            rVal.push_back(reply.msgs(i));
        }
    } else {
        cout << "SOMETHING BAD HAPPENED IN GETALLCLIENTS" << endl;
    }
    return rVal;
}

int main(int argc, char** argv) {
    string host = "lenss-comp1.cse.tamu.edu";
    string port = "4633";
    string masterHost = "lenss-comp1.cse.tamu.edu";
    string masterPort = "4632";
    bool updateFiles = false;
    int opt = 0;
    
    while ((opt = getopt(argc, argv, "h:p:m:a:f:c:")) != -1){
        switch(opt) {
            case 'h':
                host = optarg;
                break;
            case 'p':
                port = optarg;
                break;
            case 'm':
                masterHost = optarg;
                break;
            case 'a':
                masterPort = optarg;
                break;
            case 'f':
                cout << "got the flag!" << endl;
                updateFiles = optarg;
                break;
            case 'c':
                clockIndex = stoi(optarg);
                break;
            default: 
                cerr << "Worker - Invalid Command Line Argument\n";
        }
    }
    
    // initialize vector clock 
    for(int i=0; i<8; i++) {
        vectorClock.push_back(0);
    }
    
    workerHost = host;
    workerPort = port;
    workerAddress = host + ":" + port;
    masterAddress = masterHost + ":" + masterPort;
    
    pthread_t workerThread;
	pthread_create(&workerThread, NULL, RunWorker, NULL);
    
    ConnectToMaster(host, port);
    
    if(updateFiles){
        cout << "got into updateFiles" << endl;
        vector<string> clients = GetAllClients();
        cout << "got the clients: clients.size() == " << clients.size() << endl;
        for(int i = 0; i < clients.size(); i++){
            AskForFile(clients[i]);
            AskForFile(clients[i] + "following");
        }
        cout << "finished asking for files" << endl;
    }
    
    while(true) {
        continue;
    }
    
    cout << "Worker - Worker shutting down\n";
    return 0;
}