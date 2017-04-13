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

#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <time.h>
#include <unistd.h>

#include <grpc++/grpc++.h>

#include "fb.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;
using grpc::Status;
using hw4::Message;
using hw4::ListReply;
using hw4::Request;
using hw4::Reply;
using hw4::AssignedWorkers;
using hw4::MessengerMaster;
using hw4::MessengerWorker;

using namespace std;

//Helper function used to create a Message object given a username and message
Message MakeMessage(const std::string& username, const std::string& msg) {
  Message m;
  m.set_username(username);
  m.set_msg(msg);
    
  google::protobuf::Timestamp* timestamp = new google::protobuf::Timestamp();
  timestamp->set_seconds(time(NULL));
  timestamp->set_nanos(0);
  m.set_allocated_timestamp(timestamp);
  return m;
}

class MessengerClient {
    public:
    
    MessengerClient(shared_ptr<Channel> channel, string uname, string waddress){
        workerStub = MessengerWorker::NewStub(channel);
        username = uname;
        workerAddress = waddress;
    }
    
    void rerouteConnection(string address){
        workerAddress = address;
        shared_ptr<Channel> channel = grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
        workerStub = MessengerWorker::NewStub(channel);
    }
    
    void Connect() {
        if(workerStub == NULL){
            cout << "Server service has not been initialized." << endl;
            return;
        }
        
        //Data being sent to the server
        Request request;
        request.set_username(username);
  
        //Container for the data from the server
        AssignedWorkers reply;

        //Context for the client
        ClientContext context;

        Status status = workerStub->Connect(&context, request, &reply);
        
        if(status.ok()) {
            
            primaryWorker = reply.primary();
            secondaryWorker1 = reply.secondary1();
            secondaryWorker2 = reply.secondary2();
            
            // if assigned worker is not same as current address, reset worker stub
            if(primaryWorker != workerAddress) {
                rerouteConnection(primaryWorker);
            }
            
            cout << "Connecting to Assigned Worker on Address: " << primaryWorker << endl;
        }
        else {
            cout << "Error: " << status.error_code() << ": " << status.error_message() << endl;
            
            //send request to secondary worker 1 for FindPrimaryWorker() on the master
            rerouteConnection(secondaryWorker1);
            Connect();
        }
    }

    //Calls the List stub function and prints out room names
    void List() {
        
        if(workerStub == NULL){
            cout << "Master service has not been initialized." << endl;
            return;
        }
        
        //Data being sent to the server
        Request request;
        request.set_username(username);
  
        //Container for the data from the server
        ListReply list_reply;

        //Context for the client
        ClientContext context;

        Status status = workerStub->List(&context, request, &list_reply);

        //Loop through list_reply.all_rooms and list_reply.joined_rooms
        //Print out the name of each room 
        if(status.ok()) {
            cout << "All Rooms: \n";
            for(string s : list_reply.all_rooms()) {
                cout << s << endl;
            }
            cout << "Following: \n";
            for(string s : list_reply.joined_rooms()) {
                cout << s << endl;
            }
        }
        else {
            cout << status.error_code() << ": " << status.error_message()
                << endl;
            
            //send request to secondary worker 1 for FindPrimaryWorker() on the master
            rerouteConnection(secondaryWorker1);
            Connect();
            
            //re-call function for next attempt
            List();
        } 
    }

    //Calls the Join stub function and makes user1 follow user2
    void Join(const string& username2) {
        
        if(workerStub == NULL){
            cout << "Master service has not been initialized." << endl;
            return;
        }
        
        Request request;
        
        //username1 is the person joining the chatroom
        request.set_username(username);
        
        //username2 is the name of the room we're joining
        request.add_arguments(username2);

        Reply reply;

        ClientContext context;

        Status status = workerStub->Join(&context, request, &reply);

        if(status.ok()) {
            cout << reply.msg() << endl;
        }
        else {
            cout << status.error_code() << ": " << status.error_message()
                << endl;
            cout << "RPC failed\n";
            
            //send request to secondary worker 1 for FindPrimaryWorker() on the master
            rerouteConnection(secondaryWorker1);
            Connect();
            
            //re-call function for next attempt
            Join(username2);
        }
    }

    //Calls the Leave stub function and makes user1 no longer follow user2
    void Leave(const string& username2) {
        
        if(workerStub == NULL){
            cout << "Master service has not been initialized." << endl;
            return;
        }
        
        Request request;
      
        request.set_username(username);
        request.add_arguments(username2);
      
        Reply reply;
      
        ClientContext context;
      
        Status status = workerStub->Leave(&context, request, &reply);
      
        if(status.ok()) {
            cout << reply.msg() << endl;
        }
        else {
            cout << status.error_code() << ": " << status.error_message()
                << endl;
            cout << "RPC failed\n";
            
            //send request to secondary worker 1 for FindPrimaryWorker() on the master
            rerouteConnection(secondaryWorker1);
            Connect();
            
            //re-call function for next attempt
            Leave(username2);
        }
    }

    //Called when a client is run
    string Login(){
        
        if(workerStub == NULL){
            cout << "Master service has not been initialized." << endl;
            return "FAILURE";
        }
        
        Request request;

        request.set_username(username);

        Reply reply;

        ClientContext context;

        Status status = workerStub->Login(&context, request, &reply);

        if(status.ok()) {
            return reply.msg();
        }
        else {
            cout << status.error_code() << ": " << status.error_message()
                    << endl;
            return "RPC failed";
            
            //send request to secondary worker 1 for FindPrimaryWorker() on the master
            rerouteConnection(secondaryWorker1);
            Connect();
            
            //re-call function for next attempt
            Login();
        }
    }

    //Calls the Chat stub function which uses a bidirectional RPC to communicate
    void Chat (const string& messages, const string& usec) {
        const string& uname = username; 
        
        if(workerStub == NULL){
            cout << "Worker service has not been initialized." << endl;
            return;
        }
        
        ClientContext context;

        shared_ptr<ClientReaderWriter<Message, Message>> stream(workerStub->Chat(&context));

        //Thread used to read chat messages and send them to the server
        thread writer([uname, messages, usec, stream, this]() {  
            //if(usec == "n") { 
            string input = "Set Stream";

            Message m = MakeMessage(uname, input);
            stream->Write(m);

            cout << "Enter chat messages: \n";
            while(getline(cin, input)) {
                m = MakeMessage(uname, input);
                stream->Write(m);
            }
            stream->WritesDone();
            Status status = stream->Finish();
            
            if(!status.ok()) {
                cout << "Worker DIED " << status.error_code() << ": " << status.error_message()
                        << endl;

                //send request to secondary worker 1 for FindPrimaryWorker() on the master
                rerouteConnection(secondaryWorker1);
                Connect();

                //re-call function for next attempt
                Chat(messages, usec);
            }
            
            /*
            }
            else {
                string input = "Set Stream";
                
                Message m = MakeMessage(username, input);
                stream->Write(m);
                
                int msgs = stoi(messages);
                int u = stoi(usec);
                
                time_t start, end;
                cout << "Enter chat messages: \n";
                time(&start);
                
                for(int i=0; i<msgs; i++) {
                    input = "hello" + to_string(i);
                    
                    m = MakeMessage(username, input);
                    stream->Write(m);
                    
                    cout << input << '\n';
                    usleep(u);
                }
                time(&end);
                cout << "Elapsed time: " << (double)difftime(end,start) << endl;
                stream->WritesDone();
               
            }
             */
        });

        //Thread used to display chat messages from users that this client follows 
        thread reader([uname, stream, this, messages, usec]() {
            Message m;
            while(stream->Read(&m)){
                cout << m.username() << " -- " << m.msg() << endl;
            }
            Status status = stream->Finish();
            
            if(!status.ok()) {
                cout << "Worker DIED " << status.error_code() << ": " << status.error_message()
                        << endl;

                //send request to secondary worker 1 for FindPrimaryWorker() on the master
                rerouteConnection(secondaryWorker1);
                Connect();

                //re-call function for next attempt
                Chat(messages, usec);
            }
        });

        //Wait for the threads to finish
        writer.join();
        reader.join();
    }

    private:
    string username;
    string primaryWorker;
    string secondaryWorker1;
    string secondaryWorker2;
    //unique_ptr<MessengerServer::Stub> serverStub;
    unique_ptr<MessengerMaster::Stub> masterStub;
    unique_ptr<MessengerWorker::Stub> workerStub;
    string workerAddress;
};

//Parses user input while the client is in Command Mode
//Returns 0 if an invalid command was entered
//Returns 1 when the user has switched to Chat Mode
int parse_input(MessengerClient* messenger, string username, string input){

    //Splits the input on spaces, since it is of the form: COMMAND <TARGET_USER>
    size_t index = input.find_first_of(" ");

    if(index != string::npos) {
        string cmd = input.substr(0, index);
        
        if(input.length() == index+1) {
            cout << "Invalid Input -- No Arguments Given\n";
            return 0;
        }
        
        string argument = input.substr(index+1, (input.length()-index));
        
        if(cmd == "JOIN") {
            messenger->Join(argument);
        }
        else if(cmd == "LEAVE") {
            messenger->Leave(argument);
        }
        else {
            cout << "Invalid Command\n";
            return 0;   
        }
    }
    else {
        if(input == "LIST") {
            messenger->List(); 
        }
        else if(input == "CHAT") {
            //Send a request to the master to connect to a worker for chatting
            return 1;
        }
        else {
            cout << "Invalid Command\n";
            return 0;   
        }
    }
    return 0;   
}

int main(int argc, char** argv) {
    // Instantiate the client. It requires a channel, out of which the actual RPCs
    // are created. This channel models a connection to an endpoint (in this case,
    // localhost at port 50051). We indicate that the channel isn't authenticated
    // (use of InsecureChannelCredentials()).

    string hostname = "lenss-comp1.cse.tamu.edu";
    string username = "default";
    
    string port = "4633";
    string messages = "10000";
    string usec = "n";
    int opt = 0;
    
    while ((opt = getopt(argc, argv, "h:u:p:m:t:")) != -1){
        switch(opt) {
            case 'h':
                hostname = optarg;
                break;
            case 'u':
                username = optarg;
                break;
            case 'p':
                port = optarg;
                break;
            case 'm':
                messages = optarg;
                break;
            case 't':
                usec = optarg;
                break;
            default: 
                cerr << "Invalid Command Line Argument\n";
        }
    }

    string serverInfo = hostname + ":" + port;
    
    cout << "Connecting to Worker at " << serverInfo << endl;

    //Create the messenger client with the login info
    shared_ptr<Channel> channel = grpc::CreateChannel(serverInfo, grpc::InsecureChannelCredentials());
    MessengerClient *messenger = new MessengerClient(channel, username, serverInfo);
    
    //Connect to Assigned Worker
    messenger->Connect();
    
    //Call the login stub function
    cout << "\nLogging into Messenger Service\n";
    string response = messenger->Login();
    
    //If the username already exists, exit the client
    if(response == "Invalid Username") {
        cout << "Invalid Username -- please log in with a different username \n";
        return 0;
    }
    else {
        cout << response << endl;

        cout << "Enter commands: \n";
        string input;
        
        //While loop that parses all of the command input
        while(getline(cin, input)) {
            //If we have switched to chat mode, parse_input returns 1
            if(parse_input(messenger, username, input) == 1) {
                break;
            }
        }
        //Once chat mode is enabled, call Chat stub function and read input
        //messenger->StartChatMode();
        messenger->Chat(messages, usec);
    }
    return 0;
}
