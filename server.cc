#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <iostream>
#include <regex>
#include "workflow/HttpMessage.h"
#include "workflow/HttpUtil.h"
#include "workflow/WFServer.h"
#include "workflow/WFHttpServer.h"
#include "workflow/WFFacilities.h"
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
#include "defines.h"

void set_response_header(protocol::HttpResponse *resp) {
    resp->set_http_version("HTTP/1.1");
    resp->set_status_code("200");
    resp->set_reason_phrase("OK");
    resp->add_header_pair("Content-Type", "text/json");
}

void set_response_body(protocol::HttpResponse *resp, merged_result *ctx) {
    rapidjson::Document doc;
    doc.SetArray();
    rapidjson::Document::AllocatorType &allocator = doc.GetAllocator();
    std::vector<_item>::iterator iter = ctx->items.begin();
    std::vector<_item>::iterator eiter = ctx->items.end();
    for (; iter != eiter; ++iter) {
        rapidjson::Value obj;
        obj.SetObject();
        rapidjson::Value url;
        url.SetString(iter->url.c_str(), allocator);
        rapidjson::Value title;
        title.SetString(iter->title.c_str(), allocator);
        // rapidjson::Value idx;
        // idx.SetFloat(iter->idx);
        obj.AddMember("url", url, allocator);
        obj.AddMember("title", title, allocator);
        // obj.AddMember("idx", idx, allocator);
        doc.PushBack(obj, allocator);
    }

    rapidjson::StringBuffer strbuf;
    rapidjson::Writer <rapidjson::StringBuffer> writer(strbuf);
    doc.Accept(writer);

    resp->append_output_body(strbuf.GetString());
}

ParallelWork *parallel_fetch_idx(const char *body, size_t size, void *context) {

    ParallelWork *pwork = Workflow::create_parallel_work(nullptr);
    std::regex a_tag_regex("<a[^>]+?href=[\"']?([^\"']+)[\"']?[^>]*>([^<]+)</a>");

    std::cmatch match_results;

    const char *pos = strstr((char *) body, "<div id=\"body\"");
    const char *end = strstr((char *) body, "id=\"local_news\">");
    if (pos == nullptr) {
        pos = (char *) body;
    }

    if (end == nullptr) {
        end = (char *) body + size;
    }

    for (; std::regex_search(pos, end,
                             match_results, a_tag_regex);
           pos = match_results.suffix().first) {

        if (match_results.str(1).rfind("http", 0) == 0
            && !match_results.str(2).empty()) {
            // url,title
            std::string url = match_results.str(1);
            std::string title = match_results.str(2);
            // std::string url_baobian = "http://baobianapi.pullword.com:9091/get.php";
            WFHttpTask *task = WFTaskFactory::create_http_task(BAOBIAN_API,
                                                               REDIRECT_MAX,
                                                               RETRY_MAX,
                                                               [url, title](WFHttpTask *task) {
                                                                   const void *body;
                                                                   size_t size;
                                                                   task->get_resp()->get_parsed_body(&body, &size);
                                                                   rapidjson::Document d;
                                                                   d.Parse((const char *) body);
                                                                   float idx = d["result"].GetFloat();
                                                                   if (idx < -0.5) {
                                                                       _item item;
                                                                       item.title = title;
                                                                       item.url = url;
                                                                       item.idx = idx;
                                                                       merged_result *ctx = (merged_result *) series_of(
                                                                               task)->get_context();

                                                                       ctx->_mutex.lock();
                                                                       ctx->items.push_back(item);
                                                                       ctx->_mutex.unlock();
                                                                       // std::cout << title << ":" << url << ":" << idx << std::endl;
                                                                   }
                                                               });
            task->get_req()->set_method("POST");
            task->get_req()->append_output_body(title);

            SeriesWork *series = Workflow::create_series_work(task, nullptr);
            series->set_context(context);
            pwork->add_series(series);
        }
    }

    return pwork;
}

void bidu_callback(WFHttpTask *task) {
    protocol::HttpResponse *resp = task->get_resp();

    int state = task->get_state();
    if (state != WFT_STATE_SUCCESS) {
        fprintf(stderr, "[FAILED] wget %s.\n", task->get_req()->get_request_uri());
        return;
    }

    const void *body;
    size_t size;
    resp->get_parsed_body(&body, &size);

    series_of(task)->push_back(
            parallel_fetch_idx((const char *) body, size, series_of(task)->get_context()));
}

void process(WFHttpTask *server_task) {
    protocol::HttpRequest *req = server_task->get_req();
    protocol::HttpResponse *resp = server_task->get_resp();
    protocol::HttpHeaderCursor cursor(req);
    std::string name;
    std::string value;

    WFHttpTask *bidu_task = WFTaskFactory::create_http_task(BIDU_NEWS_URL,
                                                            REDIRECT_MAX,
                                                            RETRY_MAX,
                                                            bidu_callback);

    WFFacilities::WaitGroup wait_group(1);
    SeriesWork *series = Workflow::create_series_work(bidu_task, [&wait_group](const SeriesWork *) {
        wait_group.done();
    });

    // save result in ctx
    merged_result *ctx = new merged_result;
    series->set_context(ctx);
    series->start();
    // wait results
    wait_group.wait();

    // set http header & body
    set_response_header(resp);
    set_response_body(resp, ctx);

    delete ctx;
}

static WFFacilities::WaitGroup wait_group(1);

static void sig_handler(int signo) {
    wait_group.done();
}

int main(int argc, char *argv[]) {
    unsigned short port;

    if (argc != 2) {
        fprintf(stderr, "USAGE: %s <port>\n", argv[0]);
        exit(1);
    }

    signal(SIGINT, sig_handler);

    WFHttpServer server(process);
    port = atoi(argv[1]);
    if (server.start(port) == 0) {
        wait_group.wait();
        server.stop();
    } else {
        perror("Cannot start server");
        exit(1);
    }

    return 0;
}
