/*
 Copyright (C) 2011 James Coliz, Jr. <maniacbug@ymail.com>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 version 2 as published by the Free Software Foundation.
 */

#ifndef __RF24NETWORK_H__
#define __RF24NETWORK_H__

/**
 * @file RF24Network.h
 *
 * Class declaration for RF24Network
 */

#include <stdint.h>

class RF24;

/**
 * Node configuration for a specific node
 *
 * Each logical node address needs one of these.  The pipe addresses are
 * the 'physical' pipes that the underlying radio will listen/talk on.
 * Each node needs a pipe to talk to its parent, and a pipe to listen to
 * its parent.  Make sure that no pipe address is repeated anywhere in
 * the table.
 *
 * nRF24L01(+) pipe address have a peculiarity that is important to
 * keep in mind.  All nodes which share the same direct parent should
 * have the top 4 bytes of their talking pipe be identical.
 *
 * @todo The network should validate that no pipe addresses are duplicated
 * and throw an error in this case.
 */
struct RF24NodeLine
{
  uint64_t talking_pipe; /**< Pipe address for talking to parent node */
  uint64_t listening_pipe; /**< Pipe address for listening to parent node */
  uint16_t parent_node; /**< Logical address of our parent node */
};

/**
 * Marker for the start of a topology list
 */
#define RF24NODELINE_LIST_BEGIN  { 0xFFFFFFFFFFLL, 0xFFFFFFFFFFLL, 0 },

/**
 * Marker for the end of a topology list
 */
#define RF24NODELINE_LIST_END  { 0xFFFFFFFFFFLL, 0xFFFFFFFFFFLL, -1 }

/**
 * Header which is sent with each message
 *
 * The frame put over the air consists of this header and a message
 */
struct RF24NetworkHeader
{
  uint16_t from_node; /**< Logical address where the message was generated */
  uint16_t to_node; /**< Logical address where the message is going */
  uint16_t id; /**< Sequential message ID, incremented every message */
  static uint16_t next_id; /**< The message ID of the next message to be sent */

  /**
   * Default constructor
   *
   * Simply constructs a blank header
   */
  RF24NetworkHeader() {}

  /**
   * Send constructor
   *
   * Use this constructor to create a header and then send a message
   *
   * @code
   *  RF24NetworkHeader header(recipient_address);
   *  network.write(header,&message,sizeof(message));
   * @endcode
   *
   * @param _to The logical node address where the message is going
   */
  RF24NetworkHeader(uint16_t _to): to_node(_to), id(next_id++) {}

  /**
   * Create debugging string
   *
   * Useful for debugging.  Dumps all members into a single string, using
   * internal static memory.  This memory will get overridden next time
   * you call the method.
   *
   * @return String representation of this object
   */
  const char* toString(void) const;
};

/**
 * @enum rf24_direction_e Enumeration for directionality of the network
 *
 * Uni-directional networks can only talk to the base.  This is what you would set up in
 * a mesh of sensor nodes who were sending readings to the base.  Bi-directional mode
 * allows any node to address any other node, up or down any part of the tree.
 */

typedef enum { RF24_NET_UNIDIRECTIONAL = 0, RF24_NET_BIDIRECTIONAL } rf24_direction_e;

/**
 * Network Layer for RF24 Radios
 *
 * This class implements an OSI Network Layer using nRF24L01(+) radios driven
 * by RF24 library.
 */

class RF24Network
{
public:
  /**
   * Construct the network
   *
   * This requires a static topology.  Send in @p _topology as a pointer to a
   * terminated array of RF24NodeLines, one node line for each valid node address.
   * Adding a new node to the network requires adding/changing the entry in this
   * table and re-flashing the entire network.  Yes it would be nice to manage
   * this dynamically!  Someday.
   *
   * @code
   * RF24NodeLine topology[] = 
   * {
   *   RF24NODELINE_LIST_BEGIN
   *   { 0xE7E7E7E7F1LL, 0xE7E7E7E701LL, 0 }, // Node 1: Base, has no parent
   *   { 0xE7E7E7E7FELL, 0xE7E7E7E70ELL, 1 }, // Node 2: Leaf, child of #1
   *   RF24NODELINE_LIST_END
   * };
   * @endcode
   *
   * @param _radio The underlying radio driver instance
   * @param _topology Terminated array of node addresses / pipe mappings.
   *
   */
  RF24Network( RF24& _radio, const RF24NodeLine* _topology);

  /**
   * Bring up the network
   *
   * Uni-directional networks can only talk to the base.  This is what you would set up in
   * a mesh of sensor nodes who were sending readings to the base.  Bi-directional mode
   * allows any node to address any other node, up or down any part of the tree.
   *
   * In uni-directional mode, only nodes with children always need to listen.  This allows
   * the leaf nodes (those with no children) to sleep.  In bi-directional mode, all nodes
   * are listening all the time.  Listening is relatively expensive (12-13mA), so it's not
   * ideal for battery-operated nodes.
   *
   * @warning Be sure to 'begin' the radio first.
   *
   * @param _channel The RF channel to operate on
   * @param _node_address The logical address of this node
   * @param _direction Whether this is a bi- or uni-directional network.
   */
  void begin(uint8_t _channel, uint16_t _node_address, rf24_direction_e _direction );
  
  /**
   * Main layer loop
   *
   * This function must be called regularly to keep the layer going.  This is where all
   * the action happens!
   */
  void update(void);

  /**
   * Test whether there is a message available for this node
   * 
   * @return Whether there is a message available for this node
   */
  bool available(void);

  /**
   * Read a message
   *
   * @param[out] header The header (envelope) of this message
   * @param[out] message Pointer to memory where the message should be placed
   * @param maxlen The largest message size which can be held in @p message
   * @return The total number of bytes copied into @p message
   */
  size_t read(RF24NetworkHeader& header, void* message, size_t maxlen);
  
  /**
   * Send a message
   *
   * @param[in,out] header The header (envelope) of this message.  The critical
   * thing to fill in is the @p to_node field so we know where to send the
   * message.  It is then updated with the details of the actual header sent.
   * @param message Pointer to memory where the message is located 
   * @param len The size of the message 
   * @return Whether the message was successfully received 
   */
  bool write(RF24NetworkHeader& header,const void* message, size_t len);

protected:
  void open_pipes(void);
  uint16_t find_node( uint16_t current_node, uint16_t target_node );
  bool write(uint16_t);
  bool enqueue(void);

private:
  RF24& radio; /**< Underlying radio driver, provides link/physical layers */ 
  uint16_t node_address; /**< Logical node address of this unit, 1 .. UINT_MAX */
  const RF24NodeLine* topology; /**< Mapping table of logical node addresses to physical RF pipes */
  uint16_t num_nodes; /**< Number of nodes in the topology table */
  const static short frame_size = 32; /**< How large is each frame over the air */ 
  uint8_t frame_buffer[frame_size]; /**< Space to put the frame that will be sent/received over the air */
  uint8_t frame_queue[5*frame_size]; /**< Space for a small set of frames that need to be delivered to the app layer */
  uint8_t* next_frame; /**< Pointer into the @p frame_queue where we should place the next received frame */

};

/**
 * @example meshping.pde
 *
 * Example of a cross-pinging mesh network.
 * Using this sketch, each node will send a ping to one other node every
 * 2 seconds.  The RF24Network library will route the message across
 * the mesh to the correct node.
 */

/**
 * @mainpage Network Layer for RF24 Radios
 *
 * This class implements an OSI Network Layer using nRF24L01(+) radios driven
 * by the <a href="http://maniacbug.github.com/RF24/">RF24</a> library.
 *
 * @section Features Features
 *
 * The layer provides:
 * @li Host Addressing.  Each node has a logical address on the local network.
 * @li Message Forwarding.  Messages can be sent from one node to any other, and
 * this layer will get them there no matter how many hops it takes.
 *
 * The layer does not (yet) provide:
 * @li Fragmentation/reassembly.  Ability to send longer messages and put them
 * all back together before exposing them up to the app.
 * @li Dynamic topology / ad-hoc joining.  The ability to add a new node to a
 * network and have it automatically join in.
 * @li Power-efficient listening.  It would be useful for nodes who are listening
 * to sleep for extended periods of time if they could know that they would miss
 * no traffic.
 *
 * Please read through the public interface for details on how this network
 * operates.
 *
 * @section More How to learn more
 *
 * @li <a href="http://maniacbug.github.com/RF24/">RF24: Underlying radio driver</a>
 * @li <a href="http://maniacbug.github.com/RF24Network/classRF24Network.html">RF24Network Class Documentation</a>
 * @li <a href="https://github.com/maniacbug/RF24Network/">Source Code</a>
 * @li <a href="https://github.com/maniacbug/RF24Network/archives/master">Downloads Page</a>
 *
 * @section Topology Topology for Mesh Networks using nRF24L01(+)
 *
 * This network layer takes advantage of the fundamental capability of the nRF24L01(+) radio to
 * listen actively to up to 6 other radios at once.  The network is arranged in a 
 * <a href="http://en.wikipedia.org/wiki/Network_Topology#Tree">Tree Topology</a>, where
 * one node is the base, and all other nodes are children either of that node, or of another.
 * Unlike a true mesh network, multiple nodes are not connected together, so there is only one
 * path to any given node.
 *
 * @section Routing How routing is handled
 *
 * When sending a message using RF24Network::write(), you fill in the header with the logical
 * node address.  The network layer figures out the right path to find that node, and sends
 * it through the system until it gets to the right place.  This works even if the two nodes
 * are far separated, as it will send the message down to the base node, and then back out
 * to the final destination.
 *
 * All of this work is handled by the RF24Network::update() method, so be sure to call it
 * regularly or your network will miss packets.
 *
 * @section Directionality Uni-directional mode versus bi-directional mode
 *
 * In bi-directional mode, all nodes are always listening, so messages will quickly reach
 * their destination.  
 * 
 * In uni-directional mode, only parent nodes listen to their children.
 * Thus, it is impossible for a message to be sent outward.  This is useful in a case where
 * the outermost children (the leaf nodes) are operating on batteries and need to sleep.
 * This is useful for a sensor network.  The leaf nodes can sleep most of the time, and wake
 * every few minutes to send in a reading.
 *
 * In this setup, the intermediate nodes (relay nodes) need to stay powered because they
 * are always listening for messages from their children.
 *
 * In the future, I plan to write a system where messages can still be passed upward from
 * the base, and get delivered when a sleeping node is ready to receive them.  The radio
 * and underlying driver support 'ack payloads', which will be a handy mechanism for this.
 */

#endif // __RF24NETWORK_H__
// vim:ai:cin:sts=2 sw=2 ft=cpp
