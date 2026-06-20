/*
 * ISFP-Link - Network Manager (TCP Server)
 * X-Plane plugin acts as server, Python app connects as client
 */

#include "isfp_plugin.h"
#include "utils.h"
#include <mutex>
#include <sstream>
#include <iomanip>
#include <corecrt_math_defines.h>
#include "logger.h"

namespace ISFP {

    // External CSL & player data access
    extern CSLManager* g_csl;
    extern std::mutex g_player_mutex;
    extern std::vector<FlightData> g_valid_players;

    NetworkManager::NetworkManager()
        : listen_socket_(INVALID_SOCKET)
        , client_socket_(INVALID_SOCKET)
        , server_running_(false)
        , client_connected_(false)
        , port_(DEFAULT_PORT)
        , wsa_initialized_(false)
        , main_thread_callback_registered_(false) {
    }

    NetworkManager::~NetworkManager() {
        Shutdown();
    }

    // Initialize Winsock and register main thread flight loop callback
    bool NetworkManager::Initialize() {
        int result = WSAStartup(MAKEWORD(2, 2), &wsa_data_);
        if (result != 0) {
            Logger::Network("ISFP-xLink:Network:WSAStartup初始化失败\n");
            return false;
        }

        if (!main_thread_callback_registered_) {
            // Legacy SDK: void return type for flight loop registration
            XPLMRegisterFlightLoopCallback(
                &NetworkManager::MainThreadFlightLoop,
                0.1f,
                this
            );
            main_thread_callback_registered_ = true;
            Logger::Network("ISFP-xLink:MainThread:主线程回调已注册\n");
        }

        wsa_initialized_ = true;
        Logger::Network("ISFP-xLink:Network:网络管理器初始化完成\n");
        return true;
    }

    // Cleanup network resources and unregister flight loop callback
    void NetworkManager::Shutdown() {
        StopServer();
        StopRcvThread();

        if (main_thread_callback_registered_) {
            XPLMUnregisterFlightLoopCallback(&NetworkManager::MainThreadFlightLoop, this);
            main_thread_callback_registered_ = false;
            Logger::Network("ISFP-xLink:MainThread:主线程回调已注销\n");
        }

        if (wsa_initialized_) {
            WSACleanup();
            wsa_initialized_ = false;
        }

        Logger::Network("ISFP-xLink:Network:网络管理器已关闭\n");
    }

    // Queue a task to be executed on X-Plane main thread
    void NetworkManager::QueueMainThreadTask(MainThreadTaskType task_type) {
        std::lock_guard<std::mutex> lock(main_thread_mutex_);
        main_thread_tasks_.push({task_type});
        main_thread_cv_.notify_one();
        Logger::Network(("ISFP-xLink:MainThread:任务已排队，类型：" + std::to_string((int)task_type) + "\n").c_str());
    }

    // Flight loop callback: run queued main thread tasks safely
    float NetworkManager::MainThreadFlightLoop(float inElapsedSinceLastCall, float inElapsedTimeSinceLastFlightLoop, int inCounter, void* inRefcon) {
        NetworkManager* self = static_cast<NetworkManager*>(inRefcon);
        if (!self) return 0.1f;

        std::unique_lock<std::mutex> lock(self->main_thread_mutex_);
        while (!self->main_thread_tasks_.empty()) {
            MainThreadTask task = self->main_thread_tasks_.front();
            self->main_thread_tasks_.pop();
            lock.unlock();

            // Execute X-Plane API operations on main thread
            switch (task.type) {
                case MainThreadTaskType::DESTROY_ALL_AIRCRAFT:
                    if (g_csl) {
                        g_csl->DestroyAllAircraft();
                        Logger::Network("ISFP-xLink:CSL:已销毁所有飞机模型\n");
                    }
                    break;

                case MainThreadTaskType::CLEAR_PLAYER_DATA:
                    {
                        std::lock_guard<std::mutex> data_lock(g_player_mutex);
                        g_valid_players.clear();
                        Logger::Network("ISFP-xLink:CSL:已清空玩家数据列表\n");
                    }
                    {
                        std::lock_guard<std::mutex> draw_lock(g_draw_mutex);
                        g_draw_players.clear();
                        Logger::Network("ISFP-xLink:CSL:已清空绘制数据列表\n");
                    }
                    break;

                case MainThreadTaskType::DESTROY_AND_CLEAR:
                    if (g_csl) {
                        g_csl->DestroyAllAircraft();
                        Logger::Network("ISFP-xLink:CSL:已销毁所有飞机模型\n");
                    }
                    {
                        std::lock_guard<std::mutex> data_lock(g_player_mutex);
                        g_valid_players.clear();
                        Logger::Network("ISFP-xLink:CSL:已清空玩家数据列表\n");
                    }
                    {
                        std::lock_guard<std::mutex> draw_lock(g_draw_mutex);
                        g_draw_players.clear();
                        Logger::Network("ISFP-xLink:CSL:已清空绘制数据列表\n");
                    }
                    break;
            }

            lock.lock();
        }

        return 0.1f;
    }

    // Start TCP server on specified port
    bool NetworkManager::StartServer(int port) {
        std::lock_guard<std::mutex> lock(socket_mutex_);

        if (server_running_) {
            return true;
        }

        port_ = port;

        // Create TCP socket
        listen_socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listen_socket_ == INVALID_SOCKET) {
            Logger::Network("ISFP-xLink:Server:创建监听Socket失败\n");
            return false;
        }

        // Set socket reuse option
        int reuse = 1;
        setsockopt(listen_socket_, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));

        // Bind socket to port
        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(listen_socket_, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
            int err = WSAGetLastError();
            char msg[128];
            snprintf(msg, sizeof(msg), "ISFP-xLink:Server:Socket绑定端口%d失败, 错误码: %d\n", port, err);
            Logger::Network(msg);
            closesocket(listen_socket_);
            listen_socket_ = INVALID_SOCKET;
            return false;
        }

        // Start listening for connections
        if (listen(listen_socket_, 1) == SOCKET_ERROR) {
            Logger::Network("ISFP-xLink:Server:启动Socket监听失败\n");
            closesocket(listen_socket_);
            listen_socket_ = INVALID_SOCKET;
            return false;
        }

        server_running_ = true;
        server_thread_ = std::thread(&NetworkManager::ServerLoop, this);

        std::string msg = "ISFP-xLink:Server:服务已启动，端口：" + std::to_string(port) + "\n";
        Logger::Network(msg.c_str());

        return true;
    }

    // Stop TCP server and close all connections
    void NetworkManager::StopServer() {
        // Stop receive thread first to prevent stale RevData from interfering
        StopRcvThread();

        server_running_ = false;
        client_connected_ = false;

        // Close client connection (under mutex to avoid race with RevData)
        socket_mutex_.lock();
        SOCKET tmp_client = client_socket_;
        if (tmp_client != INVALID_SOCKET) {
            client_socket_ = INVALID_SOCKET;
        }
        socket_mutex_.unlock();
        if (tmp_client != INVALID_SOCKET) {
            closesocket(tmp_client);
        }

        // Close listen socket
        if (listen_socket_ != INVALID_SOCKET) {
            closesocket(listen_socket_);
            listen_socket_ = INVALID_SOCKET;
        }

#pragma warning(push)
#pragma warning(disable: 2712)
        __try {
            if (server_thread_.joinable()) {
                server_thread_.join();
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            Logger::Network("ISFP-xLink:Server:CRASH joining server thread, detaching instead\n");
            if (server_thread_.joinable())
                server_thread_.detach();
        }
#pragma warning(pop)

        QueueMainThreadTask(MainThreadTaskType::DESTROY_ALL_AIRCRAFT);
        Logger::Network("ISFP-xLink:Server:服务已停止\n");
    }

    // Server main loop: accept incoming client connections.
    // Only ONE client is allowed at any time.  If a client is already connected,
    // new connections are accepted and immediately closed (rejected) to avoid
    // expensive StopRcvThread → join → StartRcvThread churn that freezes the game.
    void NetworkManager::ServerLoop() {
        Logger::Network("ISFP-xLink:Server:服务循环已启动\n");

        while (server_running_) {
            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(listen_socket_, &readfds);

            timeval timeout;
            timeout.tv_sec = 1;
            timeout.tv_usec = 0;

            int result = select(0, &readfds, nullptr, nullptr, &timeout);

            if (result > 0 && FD_ISSET(listen_socket_, &readfds)) {
                sockaddr_in client_addr;
                int addr_len = sizeof(client_addr);
                SOCKET new_client = accept(listen_socket_, (sockaddr*)&client_addr, &addr_len);

                if (new_client != INVALID_SOCKET) {
                    // Already have a client → reject this one immediately
                    if (client_connected_) {
                        Logger::Network("ISFP-xLink:Client:客户端已连接，连接被拒绝\n");
                        closesocket(new_client);
                        continue;
                    }

                    std::lock_guard<std::mutex> lock(socket_mutex_);
                    client_socket_ = new_client;
                    client_connected_ = true;

                    // Disable Nagle's algorithm for low-latency data
                    int nodelay = 1;
                    setsockopt(client_socket_, IPPROTO_TCP, TCP_NODELAY, (char*)&nodelay, sizeof(nodelay));

                    StartRcvThread();
                    Logger::Network("ISFP-xLink:Client:客户端已连接\n");

                    // Send welcome handshake
                    std::string welcome = "{\"type\":\"connected\",\"version\":" + std::to_string(PLUGIN_VERSION) + "}\n";
                    send(client_socket_, welcome.c_str(), (int)welcome.length(), 0);
                }
            }
        }

        Logger::Network("ISFP-xLink:Server:服务循环已终止\n");
    }

    // Send flight data JSON to connected client
    bool NetworkManager::SendData(const FlightData& data) {
        if (!client_connected_ || client_socket_ == INVALID_SOCKET) {
            return false;
        }

        // Build JSON payload
        std::ostringstream json;
        json << std::fixed << std::setprecision(6);
        json << "{";
        json << "\"type\":\"flight_data\",";
        json << "\"latitude\":" << data.latitude << ",";
        json << "\"longitude\":" << data.longitude << ",";
        json << "\"altitude\":" << data.altitude << ",";
        json << "\"elevation\":" << data.elevation << ",";
        json << "\"pitch\":" << data.pitch << ",";
        json << "\"bank\":" << data.bank << ",";
        json << "\"heading\":" << data.heading << ",";
        json << "\"indicated_airspeed\":" << data.indicated_airspeed << ",";
        json << "\"true_airspeed\":" << data.true_airspeed << ",";
        json << "\"groundspeed\":" << data.groundspeed << ",";
        json << "\"vertical_speed\":" << data.vertical_speed << ",";
        json << "\"altitude_msl\":" << data.altitude_msl << ",";
        json << "\"altitude_agl\":" << data.altitude_agl << ",";
        json << "\"mag_heading\":" << data.mag_heading << ",";
        json << "\"true_heading\":" << data.true_heading << ",";
        json << "\"com1_freq\":" << data.com1_freq << ",";
        json << "\"com2_freq\":" << data.com2_freq << ",";
        json << "\"transponder\":" << data.transponder << ",";
        json << "\"squark\":\"" << data.squawk_mode << "\",";
        json << "\"gear_deploy\":" << data.gear_deploy << ",";
        json << "\"flaps_ratio\":" << data.flaps_ratio << ",";
        json << "\"throttle_ratio\":" << data.throttle_ratio;
        json << "}\n";

        std::string data_str = json.str();
        std::lock_guard<std::mutex> lock(socket_mutex_);

        int sent = send(client_socket_, data_str.c_str(), (int)data_str.length(), 0);
        if (sent == SOCKET_ERROR) {
            int error = WSAGetLastError();
            if (error == WSAECONNRESET || error == WSAECONNABORTED || error == WSAENOTCONN) {
                client_connected_ = false;
                closesocket(client_socket_);
                client_socket_ = INVALID_SOCKET;
                QueueMainThreadTask(MainThreadTaskType::DESTROY_ALL_AIRCRAFT);
                Logger::Network("ISFP-xLink:Client:客户端已断开连接\n");
            }
            return false;
        }

        return true;
    }

    // Send arbitrary JSON string to connected client
    bool NetworkManager::SendJson(const std::string& json_str) {
        if (!client_connected_ || client_socket_ == INVALID_SOCKET) {
            return false;
        }

        std::string data_str = json_str;
        if (data_str.back() != '\n') {
            data_str += '\n';
        }

        std::lock_guard<std::mutex> lock(socket_mutex_);
        int sent = send(client_socket_, data_str.c_str(), (int)data_str.length(), 0);
        if (sent == SOCKET_ERROR) {
            int error = WSAGetLastError();
            if (error == WSAECONNRESET || error == WSAECONNABORTED || error == WSAENOTCONN) {
                client_connected_ = false;
                closesocket(client_socket_);
                client_socket_ = INVALID_SOCKET;
                QueueMainThreadTask(MainThreadTaskType::DESTROY_ALL_AIRCRAFT);
                Logger::Network("ISFP-xLink:Client:客户端已断开连接\n");
            }
            return false;
        }

        return true;
    }

    // Start receiver thread for client data
    void NetworkManager::StartRcvThread() {
        if (client_connected_) {
            // Detach any previous finished thread so a new one can be created
            if (Rcv_Thread.joinable()) {
                Rcv_Thread.detach();
            }
            running_ = true;
            rcv_epoch_++;  // new epoch for this RevData instance
            int epoch = rcv_epoch_;
            Rcv_Thread = std::thread(&NetworkManager::RevData, this, epoch);
        }
    }

    // Stop receiver thread and clear buffer
    void NetworkManager::StopRcvThread() {
        running_ = false;
        // Wake up the receive thread by closing the client socket under mutex
        socket_mutex_.lock();
        SOCKET tmp_sock = client_socket_;
        if (tmp_sock != INVALID_SOCKET) {
            client_socket_ = INVALID_SOCKET;
            client_connected_ = false;
        }
        socket_mutex_.unlock();
        if (tmp_sock != INVALID_SOCKET) {
            closesocket(tmp_sock);
        }
#pragma warning(push)
#pragma warning(disable: 2712)
        __try {
            if (Rcv_Thread.joinable()) {
                Rcv_Thread.join();
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            if (Rcv_Thread.joinable())
                Rcv_Thread.detach();
        }
#pragma warning(pop)
        Rcv_Buffer.clear();
    }

    // Receive thread: read data from client
    void NetworkManager::RevData(int epoch) {
        char buffer[4096];
        std::string Tmp_Buffer;

        while (running_) {
            // If epoch changed, a newer RevData thread was started — exit immediately
            if (epoch != rcv_epoch_) break;

            // Check client_connected_ under the socket mutex for thread safety
            SOCKET current_socket;
            bool is_connected;
            {
                std::lock_guard<std::mutex> lock(socket_mutex_);
                current_socket = client_socket_;
                is_connected = client_connected_;
            }
            if (!is_connected) break;

            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(current_socket, &readfds);
            timeval timeout{1, 0};

            int Ret = select(0, &readfds, nullptr, nullptr, &timeout);
            if (Ret <= 0) continue;

            // Use the local socket copy for recv to avoid reading from a replaced socket
            int Rcv_Len = 0;
            {
                std::lock_guard<std::mutex> lock(socket_mutex_);
                // Double-check the socket hasn't been replaced since we captured current_socket
                if (client_socket_ != current_socket) continue;
                Rcv_Len = recv(current_socket, buffer, sizeof(buffer) - 1, 0);
            }

            if (Rcv_Len <= 0) {
                {
                    std::lock_guard<std::mutex> lock(socket_mutex_);
                    client_connected_ = false;
                    if (client_socket_ != INVALID_SOCKET) {
                        closesocket(client_socket_);
                        client_socket_ = INVALID_SOCKET;
                    }
                }
                running_ = false;
                QueueMainThreadTask(MainThreadTaskType::DESTROY_AND_CLEAR);
                Logger::Network("ISFP-xLink:RecvData:客户端已断开连接（接收线程）\n");
                break;
            }

            buffer[Rcv_Len] = '\0';
            Rcv_Buffer += buffer;

            size_t pos;
            while ((pos = Rcv_Buffer.find('\n')) != std::string::npos) {
                std::string line = Rcv_Buffer.substr(0, pos);
                Rcv_Buffer.erase(0, pos + 1);

                if (!line.empty()) {
                    if (g_csl_log_enabled) {
                        Logger::Network(("ISFP-xLink:RecvData:Raw data: " + line + "\n").c_str());
                    }
                    ProcessRcvJson(line);
                }
            }
        }
    }

    // Parse client JSON data and update player list
    void NetworkManager::ProcessRcvJson(const std::string& json_str) {
        try {
            json Raw_Data = json::parse(json_str);

            // Must have "type" field
            if (!Raw_Data.contains("type")) {
                Logger::Network("ISFP-xLink:JsonParse:JSON缺少type字段\n");
                return;
            }

            std::string data_type = Raw_Data["type"].get<std::string>();

            // ======= Handle EFB Query Responses =======
            if (data_type == "Send:EFB:Route") {
                std::string content = Raw_Data.contains("data") ? Raw_Data["data"].get<std::string>() : "";
                {
                    std::lock_guard<std::mutex> lock(g_efb_route_mutex);
                    g_efb_route_result = content;
                }
                Logger::Network("ISFP-xLink:EFB:航路查询结果已接收\n");
                return;
            }

            if (data_type == "Send:EFB:Weather") {
                std::string content;
                if (Raw_Data.contains("data")) {
                    if (Raw_Data["data"].is_object() || Raw_Data["data"].is_array()) {
                        content = Raw_Data["data"].dump();
                    } else {
                        content = Raw_Data["data"].get<std::string>();
                    }
                }
                {
                    std::lock_guard<std::mutex> lock(g_efb_weather_mutex);
                    g_efb_weather_result = content;
                }
                Logger::Network("ISFP-xLink:EFB:气象查询结果已接收\n");
                return;
            }

            // ======= Handle ATC / Controller Data =======
            if (data_type == "Send:ATC") {
                if (!Raw_Data.contains("data") || !Raw_Data["data"].is_array()) {
                    Logger::Network("ISFP-xLink:JsonParse:Send:ATC 数据格式无效\n");
                    return;
                }
                std::vector<ISFP::ATCData> temp_atc;
                for (const auto& atc : Raw_Data["data"]) {
                    ISFP::ATCData d{};
                    d.callsign = atc.contains("callsign") ? atc["callsign"].get<std::string>() : "";
                    d.frequency = atc.contains("frequency") ? atc["frequency"].get<std::string>() : "";
                    d.controller_type = atc.contains("type") ? atc["type"].get<std::string>() : "";
                    d.latitude = atc.contains("latitude") ? atc["latitude"].get<double>() : 0.0;
                    d.longitude = atc.contains("longitude") ? atc["longitude"].get<double>() : 0.0;
                    d.rating = atc.contains("rating") ? atc["rating"].get<int>() : 0;
                    d.valid = true;
                    if (!d.callsign.empty()) {
                        temp_atc.push_back(d);
                    }
                }
                {
                    std::lock_guard<std::mutex> lock(g_atc_mutex);
                    g_atc_list = temp_atc;
                }
                Logger::Network(("ISFP-xLink:ATC:收到管制员数据 " + std::to_string(temp_atc.size()) + " 条\n").c_str());
                return;
            }

            // ======= Handle Online Player Data =======
            if (!Raw_Data.contains("data") || !Raw_Data["data"].is_array()) {
                Logger::Network("ISFP-xLink:JsonParse:JSON数据格式无效\n");
                return;
            }

            if (data_type != "Send:OPD:FSD9" &&
                data_type != "Send:OPD:API") {
                Logger::Network("ISFP-xLink:JsonParse:未知数据类型\n");
                return;
            }

            const json& player_list = Raw_Data["data"];

            // ======= FSD9 mode: skip self-position lookup and radius filtering =======
            if (data_type == "Send:OPD:FSD9") {
                std::vector<ISFP::FlightData> temp_players;

                for (const auto& player : player_list) {
                    // Parse flight data for all players
                    ISFP::FlightData data{};
                    data.valid = true;
                    data.latitude = player["latitude"].get<double>();
                    data.longitude = player["longitude"].get<double>();
                    data.altitude_msl = player.contains("altitude") ? player["altitude"].get<double>() : 0.0;
                    data.pitch = player.contains("pitch") ? player["pitch"].get<float>() : 0.0f;
                    data.bank = player.contains("bank") ? player["bank"].get<float>() : 0.0f;
                    data.heading = player.contains("heading") ? player["heading"].get<float>() : 0.0f;
                    data.groundspeed = player.contains("ground_speed") ? player["ground_speed"].get<float>() : 0.0f;
                    data.callsign = player.contains("callsign") ? player["callsign"].get<std::string>() : "QuanQuan";
                    data.aircraft = player.contains("aircraft") ? player["aircraft"].get<std::string>() : "A319";
                    data.aircraft_family = player.contains("aircraft_family") ? player["aircraft_family"].get<std::string>() : "";
                    data.last_update_time = XPLMGetElapsedTime();

                    temp_players.push_back(data);
                }

                // Update global player data (thread-safe)
                std::lock_guard<std::mutex> lock(ISFP::g_player_mutex);
                ISFP::g_valid_players = temp_players;

                // Sync draw list
                std::lock_guard<std::mutex> draw_lock(g_draw_mutex);
                g_draw_players = temp_players;

                if (g_csl_log_enabled) {
                    Logger::Network(("ISFP-xLink:CSL:FSD9 mode - players: " + std::to_string(temp_players.size()) + "\n").c_str());
                }
                return;
            }

            // ======= API mode: requires mycid, self-position, and radius filtering =======

            if (g_csl_log_enabled) {
                Logger::Network(("ISFP-xLink:CSL:API mode - total raw players: " + std::to_string(player_list.size()) + "\n").c_str());
            }

            // Validate mycid for non-FSD9 types
            if (!Raw_Data.contains("mycid")) {
                Logger::Network("ISFP-xLink:JsonParse:缺少mycid字段\n");
                return;
            }

            // Get own CID
            int my_cid = std::stoi(Raw_Data["mycid"].get<std::string>());

            // Find own position
            double self_lat = 0.0, self_lon = 0.0;
            bool found_self = false;
            for (const auto& player : player_list) {
                if (player["cid"].get<int>() == my_cid) {
                    self_lat = player["latitude"].get<double>();
                    self_lon = player["longitude"].get<double>();
                    found_self = true;
                    break;
                }
            }
            if (!found_self) {
                Logger::Network("ISFP-xLink:JsonParse:未找到自身坐标信息\n");
                return;
            }

            if (g_csl_log_enabled) {
                Logger::Network(("ISFP-xLink:CSL:API mode - self position found: " +
                    std::to_string(self_lat) + ", " + std::to_string(self_lon) + "\n").c_str());
            }

            // Filter players by radius (4.37km max render distance)
            const double MAX_RADIUS = 4.37;
            std::vector<ISFP::FlightData> temp_players;
            int filtered_out = 0;

            for (const auto& player : player_list) {
                int player_cid = player["cid"].get<int>();
                if (player_cid == my_cid) continue;

                double p_lat = player["latitude"].get<double>();
                double p_lon = player["longitude"].get<double>();
                double dist = HaversineDistance(self_lat, self_lon, p_lat, p_lon);

                if (dist > MAX_RADIUS) {
                    if (g_csl_log_enabled) {
                        Logger::Network(("ISFP-xLink:CSL:API mode - filtered out (distance " +
                            std::to_string(dist) + "km > " + std::to_string(MAX_RADIUS) + "km): " +
                            player["callsign"].get<std::string>() + "\n").c_str());
                    }
                    filtered_out++;
                    continue;
                }

                // Parse flight data
                ISFP::FlightData data{};
                data.valid = true;
                data.latitude = p_lat;
                data.longitude = p_lon;
                data.altitude_msl = player["altitude"].get<double>();
                data.pitch = player.contains("pitch") ? player["pitch"].get<float>() : 0.0f;
                data.bank = player.contains("bank") ? player["bank"].get<float>() : 0.0f;
                data.heading = player.contains("heading") ? player["heading"].get<float>() : 0.0f;
                data.groundspeed = player.contains("ground_speed") ? player["ground_speed"].get<float>() : 0.0f;
                data.callsign = player.contains("callsign") ? player["callsign"].get<std::string>() : "QuanQuan";
                data.aircraft = player.contains("aircraft") ? player["aircraft"].get<std::string>() : "A319";
                data.aircraft_family = player.contains("family") ? player["family"].get<std::string>() : "";
                data.last_update_time = XPLMGetElapsedTime();

                temp_players.push_back(data);
            }

            if (g_csl_log_enabled) {
                Logger::Network(("ISFP-xLink:CSL:API mode - filtered out " + std::to_string(filtered_out) +
                    " players, valid: " + std::to_string(temp_players.size()) + "\n").c_str());
            }

            // Update global player data (thread-safe)
            std::lock_guard<std::mutex> lock(ISFP::g_player_mutex);
            ISFP::g_valid_players = temp_players;

            // Sync draw list
            std::lock_guard<std::mutex> draw_lock(g_draw_mutex);
            g_draw_players = temp_players;

            Logger::Network(("ISFP-xLink:CSL:有效玩家数量：" + std::to_string(temp_players.size()) + "\n").c_str());
        }
        catch (const std::exception& e) {
            Logger::Network(("ISFP-xLink:JsonParse:JSON解析失败：" + std::string(e.what()) + "\n").c_str());
        }
    }

} // namespace ISFP
