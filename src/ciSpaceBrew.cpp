//
//  ciSpaceBrew.cpp
//  EchoClientApp
//
//  Created by Ryan Bartley on 5/11/13.
//
//

#include "ciSpaceBrew.h"

using namespace std;

namespace cinder {
    namespace Spacebrew {

#pragma mark Message

string Message::getJSON( const string &configName ) const
{
    if ( type == "string" || type == "boolean" ){
        return "{\"message\":{\"clientName\":\"" + configName + "\",\"name\":\"" + name + "\",\"type\":\"" + type + "\",\"value\":\"" + value +"\"}}";
    } else {
        return "{\"message\":{\"clientName\":\"" + configName +"\",\"name\":\"" + name + "\",\"type\":\"" + type + "\",\"value\":" + value +"}}";
    }
}

bool Message::valueBoolean() const
{
    if (type != "boolean") cerr << "This Message is not a boolean type! You'll most likely get 'false'." << endl;
    return value == "true";
}

int Message::valueRange() const
{
    if ( type != "range" ) cerr << "This Message is not a range type! Results may be unpredicatable." << endl;
    return atoi(value.c_str());
}

string Message::valueString() const
{
    if (type != "string") cerr << "This Message is not a string type! Returning raw value as string." << endl;
    return value;
}

#pragma mark Config

string Config::getJSON()
{
    string message = "{\"config\": {\"name\": \"" + name +"\",\"description\":\"" + description +"\",\"publish\": {\"messages\": [";
    
    
    for (vector<Message>::iterator it = publish.begin(); it < publish.end(); it++){
        message += "{\"name\":\"" + it->name + "\",";
        message += "\"type\":\"" + it->type + "\",";
        message += "\"default\":\"" + it->value + "\"";
        message += "}";
        if ( (it + 1) < publish.end() ){
            message += ",";
        }
    }
    
    message += "]},\"subscribe\": {\"messages\": [";
    
    for (vector<Message>::iterator it = subscribe.begin(); it < subscribe.end(); it++){
        message += "{\"name\":\"" + it->name + "\",";
        message += "\"type\":\"" + it->type + "\"";
        message += "}";
        if ( (it + 1) < subscribe.end() ){
            message += ",";
        }
    }
    
    message += "]}}}";
    
    return message;
}

bool Config::operator == ( const Config & comp ){
    if ( name == comp.name && remoteAddress == comp.remoteAddress ){
        return true;
    }
    return false;
}
        
#pragma mark Connection

Connection::Connection( cinder::app::App * app, const std::string& host, const std::string& name, const std::string& description )
: app( app ),
    host( "ws://" + host + ":" + to_string(SPACEBREW_PORT) ),
    bConnected( false ),
    reconnectInterval( 2000 ),
    bAutoReconnect( false ),
    lastTimeTriedConnect( 0 ),
    config( Config( name, description ) )
{
    updateConnection = this->app->getSignalUpdate().connect( std::bind( &Connection::update, this ) ) ;
    //TODO: See if we can add a listener here to websocketpp cinder to automate this
    //WebSocketPP Interface
    client.addConnectCallback( &Connection::onConnect, this );
    client.addDisconnectCallback( &Connection::onDisconnect, this );
    client.addErrorCallback( &Connection::onError, this );
    client.addInterruptCallback( &Connection::onInterrupt, this );
    client.addPingCallback( &Connection::onPing, this );
    client.addReadCallback( &Connection::onRead, this );
    
}

Connection::~Connection()
{
    updateConnection.disconnect();
}


void Connection::update()
{
    client.poll();
    
    if ( bAutoReconnect ) {
      
        if ( !bConnected && this->app->getElapsedSeconds() - lastTimeTriedConnect > reconnectInterval) {
           
            connect( host, config );
        }
    }
}

void Connection::connect()
{
    client.connect( host );
}

void Connection::connect( const string &host, const Config &config )
{
    this->host = "ws://" + host + ":" + to_string(SPACEBREW_PORT);
    this->config = config;
    
    client.connect( this->host );
}

void Connection::send( const string &name, const string &type, const string &value )
{
    if (bConnected) {
        Message m( name, type, value );
        send(m);
    } else {
        cerr << "Send failed, not connected!" << endl;
    }
}

void Connection::sendString( const string &name, const string &value )
{
    if (bConnected) {
        Message m( name, "string", value );
        send(m);
    } else {
        cerr << "Send failed, not connected!" << endl;
    }
}

void Connection::sendRange( const string &name, int value )
{
    if ( bConnected ) {
        Message m( name, "range", to_string(value));
        send(m);
    } else {
        cerr << "Send failed, not connected!" << endl;
    }
}

void Connection::sendBoolean( const string &name, bool value )
{
    if (bConnected) {
        string out = value ? "true" : "false";
        Message m( name, "boolean", out );
        send(m);
    } else {
        cerr << "Send failed, not connected!" << endl;
    }
}

void Connection::send( const Message &m )
{
    if ( bConnected ) {
        client.write( m.getJSON( config.getName() ) );
    } else {
        cerr << "Send failed, not connected!" << endl;
    }
}

void Connection::send( Message* m )
{
    if ( bConnected ) {
        client.write( m->getJSON( config.getName() ) );
    } else {
        cerr << "Send failed, not connected!" << endl;
    }
}

void Connection::addSubscribe( const string &name, const string &type )
{
    config.addSubscribe( name, type );
    if ( bConnected ) {
        updatePubSub();
    }
}

void Connection::addSubscribe( const Message &m )
{
    config.addSubscribe(m);
    if ( bConnected ) {
        updatePubSub();
    }
}

void Connection::addPublish( const string &name, const string &type, const string &def)
{
    config.addPublish( name, type, def );
    if ( bConnected ) {
        updatePubSub();
    }
}

void Connection::addPublish( const Message &m )
{
    config.addPublish(m);
    if ( bConnected ) {
        updatePubSub();
    }
}

Config * Connection::getConfig()
{
    return &config;
}

bool Connection::isConnected()
{
    return bConnected;
}

void Connection::setAutoReconnect( bool _bAutoReconnect )
{
    bAutoReconnect = _bAutoReconnect;
}

void Connection::setReconnectRate( int reconnectMillis )
{
    reconnectInterval = reconnectMillis;
}

bool Connection::doesAutoReconnect()
{
    return bAutoReconnect;
}

void Connection::updatePubSub()
{
    client.write( config.getJSON() );
}

string Connection::getHost()
{
    return host;
}

void Connection::onConnect(  )
{
    bConnected = "true";
    updatePubSub();
}

void Connection::onDisconnect(  )
{
    bConnected = false;
    //TODO: Figure out the time
    lastTimeTriedConnect = this->app->getElapsedSeconds();
}

void Connection::onError( const string &err )
{
    cout << "error: " << err << endl;
}

void Connection::onRead( const string &stuff )
{
    Message m;
    Json::Value json;
    Json::Reader reader;
    if (reader.parse(stuff, json)) {
        
        m.name = json["message"]["name"].asString();
        m.type = json["message"]["type"].asString();
        
        if (m.type == "string" && json["message"]["value"].isString()) {
            m.value = json["message"]["value"].asString();
        } else if (m.type == "boolean") {
            if (json["message"]["value"].isInt()) {
                m.value = json["message"]["value"].asInt();
            } else if (json["message"]["value"].isString()) {
                m.value = json["message"]["value"].asString();
            }
        } else if (m.type == "range") {
            if (json["message"]["value"].isInt()) {
                m.value = json["message"]["value"].asInt();
            } else if (json["message"]["value"].isString()) {
                m.value = json["message"]["value"].asString();
            }
        }
    }
    
    onMessage(m);
}

void Connection::onInterrupt()
{
    
}

void Connection::onPing( const string &msg )
{
    
}

#pragma mark AdminConnection

AdminConnection::~AdminConnection()
{
    removeListeners();
}

//--------------------------------------------------------------
void AdminConnection::onOpen( const string &args ){
    Connection::onConnect();
    
    // send admin "register" message
    client.write("{\"admin\":[{\"admin\": true,\"no_msgs\": true}]}");
}

//--------------------------------------------------------------
void AdminConnection::onMessage( const string &args ){
    
    Message m;
    Json::Value json;
    Json::Reader reader;
    if (reader.parse(args, json)) {
        if ( json.isArray() ){
            for ( int k = 0; k < json.size(); k++ ) {
                Json::Value config = json[k];
                processIncomingJson( config );
            }
        } else if ( !json["message"].isNull() && json["message"]["clientName"].isNull()){
            Connection::onMessage(args);
        } else {
            processIncomingJson( json );
        }
    }
}

//--------------------------------------------------------------
bool AdminConnection::addRoute( const string &pub_client, const string &pub_address, const string &pub_name, const string &sub_client, const string &sub_address, const string &sub_name )
{
    //find in routes list
    RouteEndpoint pub( pub_client, pub_name, pub_address );
    
    RouteEndpoint sub( sub_client, sub_name, sub_address );
    
    bool bValidPublisher = false;
    bool bValidSubscriber = false;
    
    
    for (vector<Config>::iterator config = connectedClients.begin(); config != connectedClients.end(); config++){
        if ( config->name == pub_client && config->remoteAddress == pub_address ){
            // make sure it's a real publisher
            for ( vector<Message>::iterator publisher = config->getPublish().begin(); publisher != config->getPublish().end(); publisher++){
                if ( publisher->name == pub_name ){
                    pub.type = publisher->type;
                    bValidPublisher = true;
                    break;
                }
            }
        } else if ( config->name == sub_client &&
                   config->remoteAddress == sub_address ){
            
            // make sure it's a real subscriber
            for ( vector<Message>::iterator subscriber = config->getSubscribe().begin(); subscriber != config->getSubscribe().end(); subscriber++){
                if ( subscriber->name == pub_name ){
                    sub.type = subscriber->type;
                    bValidSubscriber = true;
                    break;
                }
            }
        }
    }
    if ( !bValidPublisher || !bValidSubscriber ){
        cerr << ( !bValidPublisher ? "Invalid publisher!\n" : "" ) << ( !bValidSubscriber ? "Invalid subscriber!" : "" ) << endl;
        return false;
    } else {
        updateRoute(ADD_ROUTE, Route( pub, sub ));
        return true;
    }
}

//--------------------------------------------------------------
bool AdminConnection::addRoute( const RouteEndpoint &pub_endpoint, const RouteEndpoint &sub_endpoint )
{
    bool bValidPublisher = false;
    bool bValidSubscriber = false;
    
    //find in routes list
    for (vector<Config>::iterator config = connectedClients.begin(); config != connectedClients.end(); config++){
        
        if ( config->name == pub_endpoint.clientName &&
            config->remoteAddress == pub_endpoint.remoteAddress ){
            // make sure it's a real publisher
            for ( vector<Message>::iterator publisher = config->getPublish().begin(); publisher != config->getPublish().end(); publisher++){
                if ( publisher->name == pub_endpoint.name ){
                    bValidPublisher = true;
                    break;
                }
            }
        } else if ( config->name == sub_endpoint.clientName &&
                   config->remoteAddress == sub_endpoint.remoteAddress ){
            
            // make sure it's a real subscriber
            for ( vector<Message>::iterator subscriber = config->getSubscribe().begin(); subscriber != config->getSubscribe().end(); subscriber++){
                if ( subscriber->name == sub_endpoint.name ){
                    bValidSubscriber = true;
                    break;
                }
            }
        }
    }
    if ( !bValidPublisher || !bValidSubscriber ){
        cerr << ( !bValidPublisher ? "Invalid publisher! " : "" ) << ( !bValidSubscriber ? "Invalid subscriber!" : "" ) << endl;
        return false;
    } else {
        updateRoute(ADD_ROUTE, Route( pub_endpoint, sub_endpoint ));
        return true;
    }
}

//--------------------------------------------------------------
bool AdminConnection::addRoute( const Route &route ){
    bool bValidRoute = false;
    
    //find in routes list
    for ( vector<Route>::iterator m_route = currentRoutes.begin(); m_route != currentRoutes.end(); m_route++){
        if ( *m_route == route ){
            bValidRoute = true;
        }
    }
    if ( !bValidRoute ){
        cerr << "Invalid route!" << endl;
        return false;
    } else {
        updateRoute( ADD_ROUTE, route );
        return true;
    }
}

//--------------------------------------------------------------
bool AdminConnection::removeRoute( const string &pub_client, const string &pub_address, const string &pub_name, const string &sub_client, const string &sub_address, const string &sub_name )
{
    //find in routes list
    RouteEndpoint pub;
    RouteEndpoint sub;
    
    bool bValidPublisher = false;
    bool bValidSubscriber = false;
    
    for (vector<Route>::iterator m_route = currentRoutes.begin(); m_route != currentRoutes.begin(); m_route++){
        if ( m_route->getPubEnd().clientName == pub_client &&
            m_route->getPubEnd().name == pub_name &&
            m_route->getPubEnd().remoteAddress == pub_address ){
            pub = m_route->getPubEnd();
            bValidPublisher = true;
        }
        if ( m_route->getSubEnd().clientName == sub_client &&
            m_route->getSubEnd().name == sub_name &&
            m_route->getSubEnd().remoteAddress == sub_address ){
            sub = m_route->getSubEnd();
            bValidSubscriber = true;
        }
    }
    if ( !bValidPublisher || !bValidSubscriber ){
        cerr << ( !bValidPublisher ? "Invalid publisher! " : "" ) << ( !bValidSubscriber ? "Invalid subscriber!" : "" ) << endl;
        return false;
    } else {
        updateRoute(REMOVE_ROUTE, Route( pub, sub ));
        return true;
    }
}

//--------------------------------------------------------------
bool AdminConnection::removeRoute( const RouteEndpoint &pub_endpoint, const RouteEndpoint &sub_endpoint ){
    bool bValidPublisher = false;
    bool bValidSubscriber = false;
    
    //find in routes list
    for (vector<Route>::iterator m_route = currentRoutes.begin(); m_route != currentRoutes.end(); m_route++){
        if ( m_route->getPubEnd() == pub_endpoint ){
            bValidPublisher = true;
        }
        if ( m_route->getSubEnd() == sub_endpoint ){
            bValidSubscriber = true;
        }
    }
    if ( !bValidPublisher || !bValidSubscriber ){
        cerr << ( !bValidPublisher ? "Invalid publisher!\n" : "" ) << ( !bValidSubscriber ? "Invalid subscriber!" : "" ) << endl;
        return false;
    } else {
        updateRoute(REMOVE_ROUTE, Route( pub_endpoint, sub_endpoint ));
        return true;
    }
}

//--------------------------------------------------------------
bool AdminConnection::removeRoute( const Route &route ){
    bool bValidRoute = false;
    
    //find in routes list
    for ( vector<Route>::iterator m_route = currentRoutes.begin(); m_route != currentRoutes.end(); m_route++){
        if ( *m_route == route ){
            bValidRoute = true;
        }
    }
    if ( !bValidRoute ){
        cerr << "Invalid route!" << endl;
        return false;
    } else {
        updateRoute(REMOVE_ROUTE, route );
        return true;
    }
}

//--------------------------------------------------------------
void AdminConnection::updateRoute( RouteUpdateType type, const Route &route ){
    
    Json::Value message;
    message["route"] = Json::Value( Json::objectValue );
    message["route"]["publisher"]   = Json::Value( Json::objectValue );
    message["route"]["subscriber"]  = Json::Value( Json::objectValue );
    
    // the JS library checks to see if stuff exists before sending...
    // not doing that for now.
    switch (type) {
        case ADD_ROUTE:
            break;
        case REMOVE_ROUTE:
            break;
    }
    
    message["route"]["type"] = Json::Value(getRouteUpdateTypeString(type));
    
    // append pub + sub
    message["route"]["publisher"]["name"]           = route.getPubEnd().name;
    message["route"]["publisher"]["type"]           = route.getPubEnd().type;
    message["route"]["publisher"]["clientName"]     = route.getPubEnd().clientName;
    message["route"]["publisher"]["remoteAddress"]  = route.getPubEnd().remoteAddress;
    
    message["route"]["subscriber"]["name"]          = route.getSubEnd().name;
    message["route"]["subscriber"]["type"]          = route.getSubEnd().type;
    message["route"]["subscriber"]["clientName"]    = route.getSubEnd().clientName;
    message["route"]["subscriber"]["remoteAddress"] = route.getSubEnd().remoteAddress;
    
    // send to server
    if ( bConnected ){
        client.write( message.toStyledString() );
    } else {
        cerr << "Send failed, not connected!" << endl;
    }
}

//--------------------------------------------------------------
void AdminConnection::processIncomingJson( const Json::Value & config ){
    
    // new connection
    if ( !config["config"].isNull() ){
        Config c( config["config"]["name"].asString(),
                  config["config"]["description"].asString(),
                  config["config"]["remoteAddress"].asString() )
        
        Json::Value publishes = config["config"]["publish"]["messages"];
        
        for ( int i = 0; i < publishes.size(); i++){
            c.addPublish(publishes[i]["name"].asString(), publishes[i]["type"].asString(), publishes[i]["default"].asString());
        }
        
        Json::Value subscribes = config["config"]["subscribe"]["messages"];
        for ( int i=0; i<subscribes.size(); i++){
            c.addSubscribe(subscribes[i]["name"].asString(), subscribes[i]["type"].asString());
        }
        
        bool bNew = true;
        
        // does this client exist yet?
        for (vector<Config>::iterator config = connectedClients.begin(); config != connectedClients.end(); config++){
            if ( *config == c)
            {
                *config = c;
                onClientUpdated(*config);
                bNew = false;
                break;
            }
        }
        
        // is this client us?
        // needs to be a better way to test this... basically just using this to add remoteAddress...
        if ( c.getJSON() == getConfig()->getJSON() ){
            getConfig()->remoteAddress = c.remoteAddress;
        }
        
        if ( bNew ){
            // doesn't exist yet, add as new
            connectedClients.push_back( c );
            onClientConnect(c);
        }
        
        // connection removed
    } else if ( !config["remove"].isNull()){
        
        for (int i=0; i < config["remove"].size(); i++){
            
            Json::Value toRemove = config["remove"][i];
            string name          = toRemove["name"].asString();
            string remoteAddress = toRemove["remoteAddress"].asString();
            
            for (int j=0; j<connectedClients.size(); j++){
                if ( connectedClients[j].name == name &&
                    connectedClients[j].remoteAddress == remoteAddress)
                {
                    onClientDisconnect(connectedClients[j]);
                    connectedClients.erase(connectedClients.begin() + j );
                    break;
                }
            }
        }
        // route
    } else if ( !config["route"].isNull()){
        
        Route r( RouteEndpoint( config["route"]["publisher"]["clientName"].asString(),
                               config["route"]["publisher"]["name"].asString(),
                               config["route"]["publisher"]["remoteAddress"].asString(),
                               config["route"]["publisher"]["type"].asString()),
                RouteEndpoint( config["route"]["subscriber"]["clientName"].asString(),
                              config["route"]["subscriber"]["name"].asString(),
                              config["route"]["subscriber"]["remoteAddress"].asString(),
                              config["route"]["subscriber"]["type"].asString()) );
        
        if ( config["route"]["type"].asString() == "add" ){
            currentRoutes.push_back(r);
            onRouteAdded(r);
        } else if ( config["route"]["type"].asString() == "remove"){
            for (int i = 0; i < currentRoutes.size(); i++){
                if ( currentRoutes[i] == r ){
                    onRouteRemoved(r);
                    currentRoutes.erase(currentRoutes.begin() + i );
                    break;
                }
            }
        }
        
        // data
    } else if ( !config["message"].isNull()){
        
        // message from admin
        if ( !config["message"]["clientName"].isNull()){
            DataMessage m( config["message"]["clientName"].asString(),
                           config["message"]["remoteAddress"].asString(),
                           config["message"]["name"].asString(),
                           config["message"]["type"].asString(),
                           config["message"]["value"].asString() );
        }
    }
}
    
    } //Spacebrew
    
} //Cinder
