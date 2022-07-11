#include <iostream>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>
#include <thread>
#include "ConservatorFrameworkFactory.h"
#include <was/blob.h>
#include <was/storage_account.h>

#include "../../generated/sample.grpc.pb.h"
#include <crow.h>

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using grpc::ChannelArguments;
using grpc::ClientAsyncResponseReader;
using grpc::CompletionQueue;
using grpc::ServerContext;
using grpc::Server;
using grpc::ServerBuilder;
using Mapreduce::Task;
using Mapreduce::Reply;
using Mapreduce::MapRequest;
using Mapreduce::ReduceRequest;
using Mapreduce::SyncReq;
using Mapreduce::TaskBeginRequest;

unique_ptr<ConservatorFramework> framework;
deque<bool> mapFinished;
deque<bool> reduceFinished;
mutex mu;

typedef struct unfinishedTask {
    int minSize;
    int maxSize;
    int nReduce;
    string container;
} unfinishedTask;

//unfinishedTask *task = nullptr;

class SyncClient final : public Task::Service {
    Status Sync(ServerContext *context, const SyncReq *request,
                Reply *reply) override {
        unique_lock<mutex> lock(mu);
        cout << "received sync msg:\n";
        if (request->mapphase()) {
            cout << "map\n";
            if (mapFinished.size() < request->mapfinished_size()) mapFinished.resize(request->mapfinished_size());
            for (int i = 0; i < request->mapfinished_size(); ++i) {
                cout << request->mapfinished(i) << " ";
                mapFinished[i] = request->mapfinished(i);
            }
        } else {
            cout << "reduce\n";
            if (reduceFinished.size() < request->reducefinished_size())
                reduceFinished.resize(request->reducefinished_size());
            for (int i = 0; i < request->reducefinished_size(); ++i) {
                cout << request->reducefinished(i) << " ";
                reduceFinished[i] = request->reducefinished(i);
            }
        }
        cout << "\n";
        return Status::OK;
    }

    Status TaskBegin(ServerContext *context, const TaskBeginRequest *request,
                     Reply *reply) override {
//        if (task) delete task;
//        task = new unfinishedTask{request->minsize(), request->maxsize(), request->nreduce(), request->containername()};
        mapFinished.clear();
        reduceFinished.clear();
        mapFinished.resize(request->nmap());
        reduceFinished.resize(request->nreduce());
        return Status::OK;
    }

    Status TaskEnd(ServerContext *context, const Reply *request,
                   Reply *reply) override {
//        delete task;
//        task = nullptr;
        mapFinished.clear();
        reduceFinished.clear();
        return Status::OK;
    }
};

class RequestClient {
public:
    RequestClient(std::shared_ptr<Channel> channel)
            : stub_(Task::NewStub(channel)) {
        auto children = framework->getChildren()->forPath("/masters");
        sort(children.begin(), children.end());
        for (int i = 1; i < children.size(); ++i) {
            followers.emplace_back(
                    Task::NewStub(
                            grpc::CreateChannel(framework->getData()->forPath("/masters/" + children[i]) + ":50051",
                                                grpc::InsecureChannelCredentials())));
        }

    }

    void
    Request(const string &containerName, int nMap, int nReduce) {
        Reply taskBeginReply;
        TaskBeginRequest req;
        req.set_nmap(nMap);
        req.set_containername(containerName);
        req.set_nreduce(nReduce);
        for (auto &stub: followers) {
            ClientContext ctx;
            stub->TaskBegin(&ctx, req, &taskBeginReply);
        }

        auto fail = atoi(std::getenv("FAIL"));

        string storage_connection_string = "DefaultEndpointsProtocol=https;AccountName=connoryin;AccountKey=PAkfxaOyc7Q+HAM0CJW20SFE6N4FviWF6wIaU5KpyJBw8jcTyCCY2U+Z80VRZUtrTGTibPXOjpob+AStkRj4PQ==;EndpointSuffix=core.windows.net";
        azure::storage::cloud_storage_account storage_account = azure::storage::cloud_storage_account::parse(
                storage_connection_string);

        azure::storage::cloud_blob_client blob_client = storage_account.create_cloud_blob_client();
        azure::storage::cloud_blob_container container = blob_client.get_container_reference(
                _XPLATSTR(containerName));

//        int id = 0;
//        int lastEnd = 0;
//        size_t sizeRemaining = minSize;

//        auto req = [&]() {
//            ClientContext context;
//            MapReply mapReply;
//            auto status = stub_->Request(&context, request, &mapReply);
//            GPR_ASSERT(status.ok());
//        };
//        auto dir = container.list_blobs();
//        auto blobs = dir->as_directory().list_blobs();
//        for (auto &blob: blobs) {
//            if (!blob.is_blob()) continue;
//            auto text_blob = blob.as_blob();
//            cout << text_blob.name() << endl;
//            assert(text_blob.exists());
//            auto size = text_blob.properties().size();
//            while (size >= sizeRemaining + lastEnd) {
//                request.add_filenames(text_blob.name());
//                request.set_requestid(id);
//                request.add_begin(lastEnd);
//                request.add_end(sizeRemaining + lastEnd);
//                request.set_containername(containerName);
//                request.set_nreduce(nReduce);
//
//                mapRequests.emplace_back(request);
//
//                request = MapRequest();
//                sizeRemaining = minSize;
//                ++id;
//                lastEnd = sizeRemaining + lastEnd;
//            }
//
//            request.add_filenames(text_blob.name());
//            request.set_requestid(id);
//            request.add_begin(lastEnd);
//            request.add_end(size);
//            request.set_containername(containerName);
//            request.set_nreduce(nReduce);
//
//            sizeRemaining -= size;
//            lastEnd = 0;
//        }

//        if (!request.filenames().empty()) {
//            mapRequests.push_back(move(request));
//        }
        for (int i = 0; i < nMap; ++i) {
            MapRequest request;
            request.set_requestid(i);
            request.set_containername(containerName);
            request.set_nreduce(nReduce);
            mapRequests.push_back(move(request));
        }

        mapFinished.resize(mapRequests.size());
        reduceFinished.resize(nReduce);

        cout << "map\n";
        for (auto &rq: mapRequests) {
            if (mapFinished.size() > rq.requestid() && mapFinished[rq.requestid()]) continue;
            if (fail && fail == rq.requestid() - 1) exit(0);
            threads.emplace_back([&]() {
                Reply mapReply;
                Status status;
                do {
                    ClientContext context;
                    context.set_deadline(std::chrono::system_clock::now() +
                                         std::chrono::minutes(2));
                    cout << "sending map task " << rq.requestid() << endl;
                    status = stub_->Map(&context, rq, &mapReply);
                    if (!status.ok()) sleep(1);
                } while (!status.ok());

                cout << "map task " << rq.requestid() << " finished\n";
                mapFinished[rq.requestid()] = true;

                SyncReq syncReq;
                syncReq.set_mapphase(true);
                for (auto &b: mapFinished) syncReq.add_mapfinished(b);
                for (auto &stub: followers) {
                    ClientContext ctx;
                    stub->Sync(&ctx, syncReq, &mapReply);
                }
            });
            std::this_thread::sleep_for(std::chrono::milliseconds(500));

        }

        for (auto &thread: threads) {
            thread.join();
        }
        threads.clear();

        Reply reduceReply;
        for (int i = 0; i < nReduce; ++i) {
//            ClientContext context;
//            Status status;
            ReduceRequest reduceRequest;
            reduceRequest.set_containername(containerName);
            reduceRequest.set_requestid(i);
            reduceRequest.set_nmap(nMap);
            reduceRequests.push_back(move(reduceRequest));
//            do {
//                status = stub_->Reduce(&context, reduceRequest, &reduceReply);
//            } while (!status.ok());
        }
        cout << "reduce" << endl;
        for (auto &rq: reduceRequests) {
            if (reduceFinished.size() > rq.requestid() && reduceFinished[rq.requestid()]) continue;
            if (fail && fail == rq.requestid() + mapFinished.size() - 1) exit(0);
            threads.emplace_back([&]() {
                Reply reduceReply;
                Status status;
                do {
                    ClientContext context;
                    context.set_deadline(std::chrono::system_clock::now() +
                                         std::chrono::minutes(2));
                    cout << "sending reduce task " << rq.requestid() << endl;
                    status = stub_->Reduce(&context, rq, &reduceReply);
                    if (!status.ok()) sleep(1);
                } while (!status.ok());
                cout << "task " << rq.requestid() << " finished\n";
                reduceFinished[rq.requestid()] = true;

                SyncReq syncReq;
                syncReq.set_mapphase(false);
                for (auto &b: mapFinished) syncReq.add_reducefinished(b);
                for (auto &stub: followers) {
                    ClientContext ctx;
                    stub->Sync(&ctx, syncReq, &reduceReply);
                }
            });
            std::this_thread::sleep_for(std::chrono::milliseconds(500));

        }

        for (auto &thread: threads) {
            thread.join();
        }

        for (auto &stub: followers) {
            ClientContext ctx;
            stub->TaskEnd(&ctx, reduceReply, &reduceReply);
        }
        mapFinished.clear();
        reduceFinished.clear();
    }

//    void AsyncCompleteRpc() {
//        void *got_tag;
//        bool ok = false;
//
//        while (cq_.Next(&got_tag, &ok)) {
//            auto *call = static_cast<AsyncClientCall *>(got_tag);
//            GPR_ASSERT(ok);
//            delete call;
//
//        }
//    }

private:
    std::unique_ptr<Task::Stub> stub_;
    CompletionQueue cq_;
    vector<thread> threads;
    vector<MapRequest> mapRequests;
    vector<ReduceRequest> reduceRequests;
    vector<std::unique_ptr<Task::Stub>> followers;
//    struct AsyncClientCall {
//        Reply reply;
//        ClientContext context;
//        Status status;
//        std::unique_ptr<ClientAsyncResponseReader<Reply>> response_reader;
//    };
};

void request(const string &containerName, int nMap, int nReduce) {
    ChannelArguments args;
    args.SetLoadBalancingPolicyName("round_robin");

    RequestClient client(
            grpc::CreateCustomChannel("dns:///worker-service:50051",
                                      grpc::InsecureChannelCredentials(), args));
//    RequestClient client(
//            grpc::CreateChannel("localhost:50051",
//                                grpc::InsecureChannelCredentials()));
//    std::thread thread_ = std::thread(&RequestClient::AsyncCompleteRpc, &client);
    client.Request(containerName, nMap, nReduce);
//    thread_.join();
}

string realPath;

void runServer() {
//    if (task) {
//        request(task->container, task->minSize, task->maxSize, task->nReduce);
//        delete task;
//        task = nullptr;
//    }

    crow::SimpleApp app;

    CROW_ROUTE(app, "/<int>/<int>/<string>")
            .methods("POST"_method)
                    ([](int nMap, int nReduce, string container) {
                        request(container, nMap, nReduce);
                        return crow::response(200);
                    });

    app.port(18080).multithreaded().run();
}

void exists_watcher_fn(zhandle_t *zh, int type,
                       int state, const char *path, void *watcherCtx) {
    cout << "exists watcher function called" << endl;
    // reset the watch
    ConservatorFramework *framework = (ConservatorFramework *) watcherCtx;
    auto children = framework->getChildren()->forPath("/masters");
    sort(children.begin(), children.end());
    for (auto &c: children) cout << c << " ";
    cout << endl;
    if (realPath == children[0]) {
        ofstream ofs("/tmp/ready");
        ofs << "0";
        runServer();
    } else {
        auto it = std::find(children.begin(), children.end(), realPath);
        assert(it != children.begin());
        int exist;
        do {
            --it;
            exist = framework->checkExists()->withWatcher(exists_watcher_fn,
                                                          watcherCtx)->forPath(
                    "/masters/" + *it);
        } while (exist != ZOK && it != children.begin());
        if (exist != ZOK) {
            ofstream ofs("/tmp/ready");
            ofs << "0";
            runServer();
        }
    }
}

int main(int argc, char **argv) {
    auto user = std::getenv("MY_POD_IP");
    ConservatorFrameworkFactory factory = ConservatorFrameworkFactory();
    framework = factory.newClient("sdc-zookeeper:2181");
    while (!framework) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        framework = factory.newClient("sdc-zookeeper:2181");
    }
    framework->start();

    framework->create()->forPath("/masters");
    int ok = framework->create()->withFlags(ZOO_EPHEMERAL | ZOO_SEQUENCE)->forPath(
            "/masters/instance", user,
            realPath);
    if (ok != ZOK) {
        cerr << "Cannot create node\n";
        framework->close();
        exit(-1);
    }
    realPath = realPath.substr(9);
    cout << "realPath: " << realPath << endl;
    auto children = framework->getChildren()->forPath("/masters");
    sort(children.begin(), children.end());
    for (auto &c: children) cout << c << " ";
    cout << endl;

    if (realPath == children[0]) {
        ofstream ofs("/tmp/ready");
        ofs << "0";
        runServer();
    } else {
        auto it = std::find(children.begin(), children.end(), realPath);
        assert(it != children.begin());
        int exist;
        do {
            --it;
            exist = framework->checkExists()->withWatcher(exists_watcher_fn,
                                                          (void *) framework.get())->forPath(
                    "/masters/" + *it);
        } while (exist != ZOK && it != children.begin());
        if (exist != ZOK) {
            ofstream ofs("/tmp/ready");
            ofs << "0";
            runServer();
        } else {
            std::string server_address("0.0.0.0:50051");
            SyncClient service;
            ServerBuilder builder;
            builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
            builder.RegisterService(&service);
            std::unique_ptr<Server> server(builder.BuildAndStart());
            cout << "follower is listening\n";
            server->Wait();
        }
    }

    framework->close();


    return 0;
}
