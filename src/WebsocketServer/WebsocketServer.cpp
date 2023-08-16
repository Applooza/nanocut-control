#define MG_ENABLE_LOG 0
#include "WebsocketServer.h"
#include "EasyRender/logging/loguru.h"

void WebsocketServer::fn(struct mg_connection *c, int ev, void *ev_data, void *fn_data)
{
    WebsocketServer *self = reinterpret_cast<WebsocketServer *>(fn_data);
    if (self != NULL)
    {
        if (ev == MG_EV_HTTP_MSG)
        {
            struct mg_http_message *hm = (struct mg_http_message *) ev_data;
            if (mg_http_match_uri(hm, "/websocket"))
            {
                char peer_ip[100];
                mg_ntoa(&c->peer, peer_ip, sizeof(peer_ip));
                LOG_F(INFO, "Websocket connection opened from %s", peer_ip);
                mg_ws_upgrade(c, hm, NULL);
                WebsocketServer::websocket_client_t client;
                client.connection = c;
                client.peer_ip = std::string(peer_ip);
                client.peer_id = "None";
                self->websocket_clients.push_back(client);
            } 
        }
        else if (ev == MG_EV_WS_MSG)
        {
            struct mg_ws_message *wm = (struct mg_ws_message *) ev_data;
            char peer_ip[100];
            mg_ntoa(&c->peer, peer_ip, sizeof(peer_ip));
            char *buff = (char*)malloc((wm->data.len  + 1) * sizeof(char));
            if (buff != NULL)
            {
                for (size_t x = 0; x < wm->data.len; x++)
                {
                    buff[x] = wm->data.ptr[x];
                    buff[x+1] = '\0';
                }
                LOG_F(INFO, "Recieved %s from %s", buff, peer_ip);
                for (size_t x = 0; x < self->websocket_clients.size(); x++)
                {
                    if (std::string(peer_ip) == self->websocket_clients[x].peer_ip)
                    {
                        if (std::string(buff) != "") self->HandleWebsocketMessage(self->websocket_clients[x], std::string(buff));
                        break;
                    }
                }
                free(buff);
            }
            else
            {
                LOG_F(ERROR, "Malloc failed to allocate memory for buffer!");
            }
            //mg_ws_send(c, wm->data.ptr, wm->data.len, WEBSOCKET_OP_TEXT);
            mg_iobuf_delete(&c->recv, c->recv.len);
        }
        else if (ev == MG_EV_CLOSE)
        {
            char peer_ip[100];
            mg_ntoa(&c->peer, peer_ip, sizeof(peer_ip));
            LOG_F(INFO, "%s closed websocket connection", peer_ip);
            for (size_t x = 0; x < self->websocket_clients.size(); x++)
            {
                if (std::string(peer_ip) == self->websocket_clients[x].peer_ip)
                {
                    self->websocket_clients.erase(self->websocket_clients.begin() + x);
                }
            }
        }
    }
}
void WebsocketServer::Send(std::string peer_id, std::string msg)
{
    for (size_t x = 0; x < this->websocket_clients.size(); x++)
    {
        if (this->websocket_clients[x].peer_id == peer_id)
        {
            mg_ws_send(this->websocket_clients[x].connection, msg.c_str(), msg.size(), WEBSOCKET_OP_TEXT);
            break;
        }
    }
}
void WebsocketServer::Send(websocket_client_t client, std::string msg)
{
    mg_ws_send(client.connection, msg.c_str(), msg.size(), WEBSOCKET_OP_TEXT);
}
void WebsocketServer::HandleWebsocketMessage(websocket_client_t client, std::string msg)
{
    LOG_F(INFO, "(WebsocketServer::HandleWebsocketMessage&%s) \"%s\"", client.peer_ip.c_str(), msg.c_str());
    try
    {
        nlohmann::json data = nlohmann::json::parse(msg.c_str());
        //Make sure id is updated for this peer
        for (size_t x = 0; x < this->websocket_clients.size(); x++)
        {
            if (this->websocket_clients[x].peer_ip == client.peer_ip)
            {
                if (this->websocket_clients[x].peer_id == "None")
                {
                    std::string id = data["id"];
                    LOG_F(INFO, "\tRegistered ip: %s with id: %s", client.peer_ip.c_str(), id.c_str());
                    this->websocket_clients[x].peer_id = data["id"];
                }
            }
        }
        //Echo any message from any client to Admin, if Admin exists
        if (data["id"] != "Admin")
        {
            this->Send("Admin", msg);
        }
        else //Message from Admin
        {
            if (data.contains("cmd"))
            {
                //cmd needs to be evaluated on the server and send results back to Admin
                nlohmann::json cmd_results;
                cmd_results["cmd"] = data["cmd"];
                if (data["cmd"] == "list_clients")
                {
                    LOG_F(INFO, "Evalutating command sent by Admin-> Sending connected clients");
                    nlohmann::json c;
                    for (size_t x = 0; x < this->websocket_clients.size(); x++)
                    {   
                        nlohmann::json o;
                        o["peer_id"] = this->websocket_clients[x].peer_id;
                        o["peer_ip"] = this->websocket_clients[x].peer_ip;
                        c.push_back(o);
                    }
                    cmd_results["results"] = c;
                }
                this->Send("Admin", cmd_results.dump());
            }
            else
            {
                //Make sure messages recieved from Admin are sent to propper peer which Admin specified
                this->Send(data["peer_id"], msg);
            }
        }
    }
    catch(const std::exception& e)
    {
        LOG_F(INFO, "(WebsocketServer::HandleWebsocketMessage) %s", e.what());
    }
}
void WebsocketServer::Init()
{
    const char *listen_on = "http://162.216.17.199:8000";
    mg_mgr_init(&this->mgr);
    mg_http_listen(&mgr, listen_on, this->fn, this);
    LOG_F(INFO, "Starting websocket server on: %s", listen_on);
}
void WebsocketServer::Poll()
{
    mg_mgr_poll(&this->mgr, 1);
}
void WebsocketServer::Close()
{
    mg_mgr_free(&this->mgr);
}