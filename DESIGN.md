# RealMesh - Design Decisions

## Project Overview
RealMesh is a simplified, efficient mesh networking system for Heltec V3 devices on EU 868MHz, designed to solve network congestion issues in existing mesh solutions.

## Core Architecture

### Node Addressing & Subdomains
- **Format**: `nodeID@subdomain`
- **Examples**: 
  - `nicole1@beograd` (mobile)
  - `node1@zeleznik` (stationary) 
  - `node2@zeleznik` (mobile)
- **Purpose**: Enables hierarchical routing and local network optimization
- **Self-assignment**: Nodes choose their own nodeID and subdomain names
- **Uniqueness enforcement**: Nodes within same subdomain must have unique nodeID
  - Existing @subdomain nodes notify new joiners of name conflicts
  - Conflicted nodes must change their nodeID before being accepted
  - Only nodes with unique names participate in routing

### Node Types
- **Stationary**: Act as routing hubs, maintain comprehensive path tables
- **Mobile**: Use flood when moving, learn paths when static
- **Behavior**: Mobile nodes can temporarily act stationary when settled

## Routing Strategy

### Path Memory & Expiry
- **Duration**: Nodes remember paths indefinitely until they fail
- **Failure handling**: After 2-3 failed direct attempts, fallback to flood routing
- **Path updates**: Successful routes via intermediary nodes update path tables

### Message Retry Logic
1. **First attempt**: Direct path (if known)
2. **Second attempt**: Direct path with "second retry" flag
3. **Intermediary help**: Stationary nodes in target subdomain assist on "second retry"
4. **Path learning**: Successful intermediary routes become new primary paths
5. **Third+ attempts**: Flood routing

### Subdomain Routing Intelligence
When `node1@sremcica` → `node2@zeleznik`:
1. Direct attempt fails
2. Second retry with subdomain flag
3. `node1@zeleznik` sees message for @zeleznik subdomain
4. `node1@zeleznik` forwards to `node2@zeleznik`
5. Success acknowledgment updates path: `node1@sremcica` → `node1@zeleznik` → `node2@zeleznik`
6. Future messages use learned intermediary path

### Path Storage
- **Primary path**: Most recent successful route
- **Backup path**: Previous working route (kept for redundancy)
- **Intermediary memory**: Stationary nodes remember which connections they facilitated
  - Track: "I am the bridge between nodeA@domainX and nodeB@domainY"
  - Enables proactive routing optimization and faster path discovery

### Path Table Management
- **Stationary nodes**: Remember as many destinations as memory allows (high capacity)
- **Mobile nodes**: Remember paths but with different failure handling:
  - After first direct attempt fails → immediate flood fallback
  - Less aggressive path caching to account for mobility

## Message Types

### 1. Data Messages
- **Direct**: Point-to-point with known path
- **Subdomain**: Targeted to subdomain with intermediary help
- **Flood**: Broadcast when direct methods fail

### 2. Control Messages
- **Heartbeat**: Periodic topology announcements
- **ACK/NACK**: Delivery confirmations
- **Route Discovery**: Path finding requests
- **Path Update**: Successful route notifications

### 3. Priority Levels
1. **Emergency**: Immediate transmission, can interrupt other traffic
2. **Direct**: Personal messages, higher priority than public
3. **Public Chat**: General channel, lowest priority
4. **Control**: Network maintenance, scheduled transmission

## Message Queuing & Dropping

### Queue Management
- **Emergency queue**: Never drops, always transmits first
- **Direct queue**: Limited size (e.g., 10 messages), FIFO with age-based dropping
- **Public queue**: Small size (e.g., 5 messages), aggressive dropping
- **Control queue**: Scheduled, can be delayed but not dropped

### Dropping Strategies
- **Age-based**: Drop messages older than threshold (e.g., 5 minutes for chat)
- **Priority-based**: Drop lower priority when queues full
- **Duplicate detection**: Drop repeated messages (based on hash)
- **Congestion response**: Increase dropping aggressiveness when network busy

## Network Discovery

### New Node Join
1. **Initial announcement**: Broadcast chosen `nodeID@subdomain`
2. **Uniqueness check**: Existing @subdomain nodes respond if name conflict exists
3. **Name resolution**: If conflict, node must choose new unique nodeID
4. **Acceptance**: Once unique, node is accepted into subdomain routing
5. **Topology discovery**: Receive routing information from nearby stationary hubs
6. **Integration**: Gradually build routing table through message attempts

### Topology Maintenance
- **Stationary node heartbeats**: Every 5-10 minutes, broadcast:
  - Own existence and status
  - List of nodes in direct contact within their subdomain
  - Available paths to other subdomains they can bridge
  - "I am a stationary hub for @subdomain" announcements
- **Mobile node heartbeats**: Less frequent, mainly presence announcements
- **Event-driven updates**: When routes change or nodes move
- **Topology sharing**: Stationary nodes act as subdomain routing authorities

## Congestion Management

### Back-off Algorithms
- **Exponential back-off**: For failed transmissions
- **Random jitter**: Prevent synchronized retransmissions
- **Network sensing**: Listen before transmit

### Load Balancing
- **Path selection**: Use backup paths when primary is congested
- **Subdomain distribution**: Route through different subdomains when possible
- **Time-based spreading**: Delay non-urgent messages during peak times

## Technical Implementation Notes

### For PoC
- Single radio profile (125kHz, SF7-SF9)
- Focus on routing intelligence over physical optimization
- Heltec V3 ESP32 target platform
- EU 868MHz frequency band

### Future Enhancements
- Multiple transmission profiles
- Advanced congestion algorithms
- Mobile node movement prediction
- Cross-frequency coordination

---

## Security & Authentication

### Message Authentication
- **Direct messages**: Encrypted using public-private key pairs
- **Public messages**: Unencrypted (broadcast nature)
- **Node verification**: Trust-based on acknowledgment responses
  - If node acknowledges re-broadcast, we trust their routing capability
  - Failed acknowledgments reduce trust and trigger alternate routing

### Encryption Strategy
- **Private messages only**: Direct node-to-node communication encrypted
- **Public channels**: Remain unencrypted for network efficiency
- **Key exchange**: Nodes exchange public keys during initial handshake

## Message Packet Format

### Header Structure
- **Message ID**: 32-bit unique identifier for tracking/deduplication
- **Source**: Full nodeID@subdomain address
- **Destination**: Target nodeID@subdomain (or @subdomain for local broadcast)
- **Message Type**: DATA/CONTROL/HEARTBEAT/ACK/EMERGENCY
- **Priority**: EMERGENCY/DIRECT/PUBLIC/CONTROL
- **Routing Flags**: DIRECT/SUBDOMAIN_RETRY/FLOOD/INTERMEDIARY_ASSIST
- **Hop Count**: Tracks message propagation distance
- **Timestamp**: For age-based dropping and ordering

### Payload Limits
- **Maximum payload**: 200 bytes (allows for header + substantial message content)
- **Header overhead**: ~32 bytes
- **Effective message size**: ~168 bytes for user content

### Message ID Generation
- **Format**: NodeID hash + timestamp + sequence counter
- **Collision prevention**: 32-bit space with temporal and spatial uniqueness
- **Tracking**: Used for ACK matching and duplicate detection

### Routing Metadata
- **Path history**: Last 3 hops for loop prevention
- **Quality metrics**: Signal strength, retry count
- **Timing info**: Transmission timestamp for latency measurement

## Conflict Resolution

### Name Conflict & Node Identity
- **Persistent identity**: Each node generates hidden UUID on first boot
- **Format**: `nikdale1@cukarica_a81f` (public name + hidden suffix)
- **Conflict resolution**: 
  - Active node always wins naming rights
  - Offline nodes lose name after 72 hours of inactivity
  - Returning nodes reclaim name using their persistent UUID
  - UUID prevents impersonation of returning nodes

### Multiple Stationary Hubs
- **Cooperative approach**: Multiple stationary hubs in same subdomain is beneficial
- **Load distribution**: Both can rebroadcast and serve as routing points
- **Redundancy**: Backup routing capability if one hub fails
- **Coordination**: Hubs share routing tables and coordinate to prevent loops

### Tie-breaking Mechanisms
- **Signal strength**: Stronger signal wins in routing decisions
- **Response time**: Faster ACK responses get preference
- **Hop count**: Shorter paths preferred
- **Node age**: Older established nodes get slight preference

## Network Bootstrap & Recovery

### Cold Start
- **First node behavior**: Acts as subdomain founder, accepts all new joiners
- **Cross-subdomain discovery**: Actively seeks other subdomains for bridging
- **Authority establishment**: Becomes initial routing authority for subdomain

### Network Joining
- **Retry strategy**: 3 attempts with exponential backoff (1s, 3s, 9s)
- **Fallback**: If no response, assume first node in subdomain
- **Discovery period**: 30 seconds of listening before claiming founder status

### Isolation Recovery
- **Reconnection**: Use same UUID-based identity reclaim process
- **Route rebuild**: Gradually rediscover paths through message attempts
- **Priority recovery**: Emergency and direct messages get routing priority during recovery

## Emergency Messages (PoC Simplified)
- **Qualification**: Any message marked emergency by any node
- **Authorization**: No restrictions for PoC - anyone can send emergency
- **Validation**: No spam prevention for PoC (trust-based system)
- **Behavior**: Immediate transmission, interrupts other traffic, flood routing

## Technical Parameters

### Radio Configuration
- **Frequency**: 868MHz (EU band)
- **Bandwidth**: 125kHz (maximum for best range)
- **Spreading Factor**: SF12 (maximum for best sensitivity)
- **Coding Rate**: 4/5 (good error correction)
- **TX Power**: 20dBm (maximum allowed)
- **Preamble**: 8 symbols
- **Sync Word**: 0x12 (private network)

### Memory & Timing
- **Routing table**: Use all available RAM (typically 4000+ entries on ESP32)
- **ACK timeout**: 10 seconds for direct, 30 seconds for flood
- **Retry intervals**: 5s, 15s, 45s (exponential backoff)
- **Heartbeat period**: 300 seconds (5 minutes) for stationary, 900 seconds (15 minutes) for mobile
- **Message aging**: Drop chat messages older than 10 minutes

### Range & Network Assumptions
- **Line of sight**: ~5-15km depending on terrain
- **Urban environment**: ~1-3km with obstacles
- **Network diameter**: Assume max 10 hops for any message
- **Congestion threshold**: >80% channel utilization triggers aggressive dropping

## Graceful Degradation (PoC Strategy)

### Overload Handling
- **Queue prioritization**: Emergency > Direct > Public > Control
- **Aggressive dropping**: Drop public chat first, then older messages
- **Back-pressure**: Increase retry delays when network busy

### Partial Failures
- **Route invalidation**: Remove failed paths immediately
- **Fallback strategy**: Direct → Intermediary → Flood
- **Recovery**: Gradual path rediscovery through successful message attempts

### Network Partition Recovery
- **Automatic bridging**: Nodes actively seek cross-subdomain connections
- **Route advertisement**: Stationary hubs broadcast bridge capabilities
- **Healing**: Network naturally heals as mobile nodes move between partitions

## Open Questions Remaining

1. **Cross-subdomain optimization**: Advanced routing between distant subdomains
2. **Performance tuning**: Real-world optimization of timing parameters
3. **Scale testing**: Behavior with 100+ nodes in single subdomain

## Next Steps

1. Define message packet format
2. Implement basic LoRa radio layer
3. Create routing table data structures
4. Develop path learning algorithms
5. Build congestion management system