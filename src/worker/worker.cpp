#include <iostream>
#include <memory>
#include <string>

#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include <glog/logging.h>
#include <thread>
#include <sys/wait.h>

#include <was/blob.h>
#include <was/storage_account.h>

#include "../../generated/sample.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using Mapreduce::Task;
using Mapreduce::Reply;
using Mapreduce::MapRequest;
using Mapreduce::ReduceRequest;

#include "ConservatorFrameworkFactory.h"

class GreeterServiceImpl final : public Task::Service {
    azure::storage::cloud_blob_client blob_client;

    Status Map(ServerContext *context, const MapRequest *request,
               Reply *reply) override {
        LOG(INFO) << "Server: " << std::getenv("MY_POD_IP")
                  << " got map request with ID: " << request->requestid() << "\n";
        if (++served == fail) exit(0);

        azure::storage::cloud_blob_container container = blob_client.get_container_reference(
                _XPLATSTR(request->containername()));

//        for (int i = 0; i < request->filenames_size(); ++i) {
        azure::storage::cloud_block_blob text_blob = container.get_block_blob_reference(
                _XPLATSTR("input/" + to_string(request->requestid())));
        assert(text_blob.exists());
        concurrency::streams::container_buffer<std::vector<uint8_t>> buffer;
        concurrency::streams::ostream output_stream(buffer);
        text_blob.download_to_stream(output_stream);
        string input = string(buffer.collection().begin(), buffer.collection().end());
//            size_t begin = request->begin(i), end = request->end(i);
//            if (begin) while (isgraph(text[begin])) ++begin;
//            while (end < text.size() - 1 && isgraph(text[end])) ++end;
//            text = text.substr(begin, end - begin);
//            input.append(text);
//        }

        string filename =
                request->containername() + "_map_" + to_string(request->requestid());
        ofstream ofs(filename);
        ofs << input;
        ofs.close();

        int id = fork();
        if (!id) execlp("python3", "python3", "mapper.py", filename.c_str(), nullptr);
        else {
            int status;
            wait(&status);
            ifstream ifs(filename + "_out");
            string key, val;
            hash<string> hasher;
            vector<string> outFiles(request->nreduce());

            while (ifs >> key) {
                ifs >> val;
                outFiles[hasher(key) % request->nreduce()].append(key + " " + val + "\n");
            }
            for (int i = 0; i < request->nreduce(); ++i) {
                azure::storage::cloud_block_blob blob2 = container.get_block_blob_reference(
                        _XPLATSTR("temp/" + to_string(request->requestid()) + "_" +
                                  std::to_string(i)));
                cout << "uploading file " << to_string(request->requestid()) + "_" +
                                             std::to_string(i) << " of size " << outFiles[i].size() << endl;
                blob2.upload_text(_XPLATSTR(outFiles[i]));
            }

            return Status::OK;
        }

//        int inputPipe[2], outputPipe[2];
//        pipe(inputPipe);
//        pipe(outputPipe);

//        int pid = fork();
//        if (pid == 0) {
//            // replace standard input with input part of pipe
//            dup2(inputPipe[1], 0);
//            // close unused hald of pipe
////            close(inputPipe[0]);
//            // execute grep
//            execlp("python3", "python3", "mapper.py"
////               , (to_string(inputPipe[1]) + ">&0").c_str()
////               ,(">&" + to_string(fd[1])).c_str()
//                    , nullptr);
//        } else {
////            close(inputPipe[1]);
//            write(inputPipe[0], input.c_str(), input.size() + 1);
////            close(inputPipe[0]);
//            sleep(10);
//            return Status::OK;
//
//        }
//
//
//        string output(input.size() * 2, '\0');
//        long nbytes = read(outputPipe[1], output.data(), output.size());
//        LOG(INFO) << "buffer size: " << output.size() << ", read bytes: " << nbytes
//                  << "\n";
//        output.resize(nbytes);
//
//        azure::storage::cloud_block_blob blob2 = container.get_block_blob_reference(
//                _XPLATSTR("temp/" + std::to_string(request->requestid())));
//        blob2.upload_text(_XPLATSTR(output));
    }

    Status Reduce(ServerContext *context, const ReduceRequest *request,
                  Reply *reply) override {
        LOG(INFO) << "Server: " << std::getenv("MY_POD_IP")
                  << " got reduce request with ID: " << request->requestid() << "\n";
        if (++served == fail) exit(0);

        azure::storage::cloud_blob_container container = blob_client.get_container_reference(
                _XPLATSTR(request->containername()));

        string filename =
                request->containername() + "_reduce_" + to_string(request->requestid());

        ofstream ofs(filename);
        string tmp, key, val;
        for (int i = 0; i < request->nmap(); ++i) {
            azure::storage::cloud_block_blob blob2 = container.get_block_blob_reference(
                    _XPLATSTR("temp/" + to_string(i) + "_" +
                              to_string(request->requestid())));
            tmp.append(blob2.download_text());
        }
        istringstream iss(tmp);
        map<string, vector<string>> mapping;
        while (iss >> key) {
            iss >> val;
            mapping[key].emplace_back(val);
        }
        for (auto &word: mapping) {
            ofs << word.first << " ";
            for (auto &v: word.second) {
                ofs << v << " ";
            }
            ofs << "\n";
        }

        int id = fork();
        if (!id) execlp("python3", "python3", "reducer.py", filename.c_str(), nullptr);
        else {
            int status;
            wait(&status);
            ifstream ifs(filename + "_out");
            ifs.seekg(0, std::ios::end);
            size_t size = ifs.tellg();
            std::string buffer(size, ' ');
            ifs.seekg(0);
            ifs.read(&buffer[0], size);
            cout << "uploading buffer of size " << size << endl;
            azure::storage::cloud_block_blob blob = container.get_block_blob_reference(
                    _XPLATSTR("output/" + to_string(request->requestid())));
            blob.upload_text(_XPLATSTR(buffer));

            return Status::OK;
        }
    }

    int fail;
    atomic<int> served{0};

public:
    GreeterServiceImpl() {
        string storage_connection_string = "DefaultEndpointsProtocol=https;AccountName=connoryin;AccountKey=PAkfxaOyc7Q+HAM0CJW20SFE6N4FviWF6wIaU5KpyJBw8jcTyCCY2U+Z80VRZUtrTGTibPXOjpob+AStkRj4PQ==;EndpointSuffix=core.windows.net";
        azure::storage::cloud_storage_account storage_account = azure::storage::cloud_storage_account::parse(
                storage_connection_string);
        blob_client = storage_account.create_cloud_blob_client();
        fail = atoi(std::getenv("FAIL"));
    }
};

void RunServer() {
    std::string server_address("0.0.0.0:50051");
    GreeterServiceImpl service;

//    grpc::EnableDefaultHealthCheckService(true);
//    grpc::reflection::InitProtoReflectionServerBuilderPlugin();
    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<Server> server(builder.BuildAndStart());
    server->Wait();
}

int main(int argc, char **argv) {
    google::InitGoogleLogging(argv[0]);
    FLAGS_alsologtostderr = 1;

    LOG(INFO) << "Server listening on " << std::getenv("MY_POD_IP") << std::endl;

    RunServer();

    return 0;
}