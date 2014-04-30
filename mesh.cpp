
#include "Mesh.h"
#include "RF24.h"
#include "timer.h"

// Delay manager in ms
timer_t ping_timer(30000);

// Debug info
#define DEBUG   false

/****************************************************************************/

Mesh::Mesh( RF24& _radio ): radio(_radio), network(radio) {}

/****************************************************************************/

void Mesh::begin(uint8_t _channel, uint16_t _node_id)
{
  // save settings
  node_id = _node_id;
  channel = _channel;
  // is it base node?
  if( node_id == base ) {
    node_address = base;
  }
  // otherwise it is broadcast
  else {
    node_address = broadcast;
  }
  if(DEBUG) printf_P(PSTR("MESH: Info: Initializing Node: id: %u, address: 0%o.\n\r"),
              node_id, node_address);
  // set address and channel
  network.begin(channel, node_address);
}

/****************************************************************************/

bool Mesh::ready()
{
  return ready_to_send;
}

/****************************************************************************/

bool Mesh::send(Payload& payload, uint16_t to_id)
{
  if(to_id != base && nodes.contains(to_id) == false) {
    // we should know about this node
    return false;
  }
  // set payload id from sender id
  payload.id = node_id;
  // get destination address
  uint16_t to_address = nodes[to_id];
  RF24NetworkHeader header(to_address, 'M'); header.from_node = node_address;
  if(DEBUG) printf_P(PSTR("MESH: Info: %u, %s, %d byte: Sending payload to %u: %s..."), 
              node_id, header.toString(), sizeof(payload), to_id, payload.toString());
              
  bool ok = network.write(header,&payload,sizeof(payload));
  if(ok) {
    if(DEBUG) printf_P(PSTR(" ok.\n\r"));
    return true;
  // is it base?
  } else if(node_id == base) {
    if(DEBUG) printf_P(PSTR(" failed!\n\r"));
    // delete bad node
    nodes.remove(to_id);
    if(DEBUG) printf_P(PSTR("MESH: Info: %u: Node '%u' id is removed from address map!: %s.\n\r"), 
                node_id, to_id, nodes.toString());
    if(nodes.size() == 0)
      ready_to_send = false;
    return false;
  // otherwise reset our node
  } else {
    if(DEBUG) printf_P(PSTR(" failed! Reset!\n\r"));
    reset_node();
    return false;
  }
}

/****************************************************************************/

const char* Payload::toString() const {
  static char buffer[30];
  snprintf_P(buffer,sizeof(buffer),PSTR("%d {%s=%d}"), id, key, value);
  return buffer;   
}

/****************************************************************************/

void Mesh::update()
{
  // update RF24Network
  network.update();
  
  // Is there anything ready?
  if ( network.available() ) {
    RF24NetworkHeader header;
    network.peek(header);

    if(DEBUG) printf_P(PSTR("MESH: Info: %u, 0%o: Got message %c from 0%o.\n\r"),
                node_id, node_address, header.type, header.from_node);
    // Dispatch the message to the correct handler.
    switch (header.type) {
      case 'P':
        handle_P(header);
        break;
      case 'A':
        handle_A(header);
        break;
      case 'M':
        // do not handle payload
        break;
      default:
        // Unknown message type
        // skip this message
        network.read(header,0,0);
        printf_P(PSTR("MESH: Error: %u, %s: Unknown message type!\n\r"),
                node_id, header.toString());
        break;
    };
  }
    
  if ( ping_timer ) {
    // is it broadcaster?
    if(node_address == broadcast) {
      //Broadcast ping
      send_P();
    }
  }
}

/****************************************************************************/

bool Mesh::available()
{
  if( network.available() ) {
    RF24NetworkHeader header;
    network.peek(header);
    if(header.type == 'M') {
      return true;
    }
  }
  return false;
}

/****************************************************************************/

void Mesh::read(Payload& payload)
{
  RF24NetworkHeader header;
  network.peek(header);
  if(header.type == 'M') {
    uint8_t received = network.read(header,&payload,sizeof(payload));
    if(DEBUG) printf_P(PSTR("MESH: Info: %u, %s, %d byte: Received payload: %s.\n\r"),
                node_id, header.toString(), received, payload.toString());
  }
}

/****************************************************************************/

bool Mesh::send_P()
{
  RF24NetworkHeader header(broadcast, 'P'); header.from_node = node_address; 
  if(DEBUG) printf_P(PSTR("MESH: Info: %u, %s, %d byte: Broadcast ping..."), 
              node_id, header.toString(), sizeof(node_id));
  bool ok = network.write(header,&node_id,sizeof(node_id));
  if(ok) {
    if(DEBUG) printf_P(PSTR(" ok.\n\r"));
  } else {
    if(DEBUG) printf_P(PSTR(" failed!\n\r"));
  }
  return ok;
}

/****************************************************************************/

void Mesh::handle_P(RF24NetworkHeader& header)
{
  uint16_t id;
  uint8_t received = network.read(header,&id,sizeof(id));
  if(DEBUG) printf_P(PSTR("MESH: Info: %u, %s, %d byte: Received ping from %d.\n\r"),
              node_id, header.toString(), received, id);
  // is it our ping?
  if(node_id == id) {
    // skip if it ours.
    return;
  }
  // who send ping?
  if(header.from_node == broadcast) {
    uint16_t leaf_address;
    // do we known this node?
    if(nodes.contains(id)) {
      leaf_address = nodes[id];
    } else {
      // find empty slot for new leaf
      leaf_address = get_leaf_address();
    }

      // send new address to broadcaster
    send_A(header.from_node, leaf_address);
    return;
  }
  // if node isn't broadcaster we should add its to our network
  // add new or update existing node
  nodes[id] = header.from_node;
  if(DEBUG) printf_P(PSTR("MESH: Info: %u, 0%o: Address map is updated: %s.\n\r"), 
              node_id, node_address, nodes.toString());
  // change network state
  ready_to_send = true;
}

/****************************************************************************/

bool Mesh::send_A(const uint16_t to_address, const uint16_t leaf_address)
{
  RF24NetworkHeader header(to_address, 'A'); header.from_node = node_address;
  if(DEBUG) printf_P(PSTR("MESH: Info: %u, %s, %d byte: Sending new leaf address '0%o'.\n\r"), 
              node_id, header.toString(), sizeof(leaf_address), leaf_address);
  return network.write(header,&leaf_address,sizeof(leaf_address));
}

/****************************************************************************/

void Mesh::handle_A(RF24NetworkHeader& header)
{
  uint16_t new_address;
  uint8_t received = network.read(header,&new_address,sizeof(new_address));
  if(DEBUG) printf_P(PSTR("MESH: Info: %u, %s, %d byte: Received address '0%o'.\n\r"),
              node_id, header.toString(), received, new_address);
  // reinitialize node address
  set_address(new_address);
}

/****************************************************************************/

uint16_t Mesh::get_leaf_address()
{
  bool exists = false;
  // shifting from 055 to 0550
  uint16_t shift = node_address << 3; 
  // check for available spaces
  if(shift >= broadcast)
    return broadcast;
  // for node 052 will return 0521-0525
  for(uint16_t address = shift+1; address <= shift+5; address++) {
    for(int index = 0; index < nodes.size(); index++) {
      if(nodes.valueAt(index) == address) {
         exists = true;
      }
    }
    // we found empty slot
    if(exists == false)
      return address;
    }
  // nothing for this relay




  return broadcast;
}

/****************************************************************************/

void Mesh::set_address(uint16_t address)
{
  // update setting
  node_address = address;
  if(DEBUG) printf_P(PSTR("MESH: Info: Reinitializing Node: id: %u, new address: 0%o.\n\r"),
              node_id, node_address);
  // apply new address
  network.begin(channel, node_address);
  // send ping to broadcast
  bool ok = send_P();
  if(ok) {
    // change connection state
    ready_to_send = true;
  } else {
    // reset settings
    reset_node();
  }
}

/****************************************************************************/

void Mesh::reset_node()
{ 
  // clear knowing nodes
  for(int index=0; index<nodes.size(); index++) {
    nodes.remove(index);
  }
  // change connection state
  ready_to_send = false;
  // set address as broadcast
  node_address = broadcast;
  if(DEBUG) printf_P(PSTR("MESH: Info: %u, 0%o: Node is flashed.\n\r"), 
              node_id, node_address);
  // reinitialize node
  network.begin(channel, node_address);
}

/****************************************************************************/
