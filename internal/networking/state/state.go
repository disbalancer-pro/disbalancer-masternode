package state

import (
	"log"
	"net"
	"strings"
	"sync"

	"github.com/gladiusio/gladius-masternode/internal/http"
	"github.com/hongshibao/go-kdtree"
	"github.com/tidwall/gjson"
)

// NetworkState is a struct containing a K-D tree representation of the edge node pool
// associated with this masternode
type NetworkState struct {
	tree *kdtree.KDTree // K-D Tree to store network node structs

	running    bool
	runChannel chan (bool)
	mux        sync.Mutex
}

// NewNetworkState returns a new NetworkState struct
func NewNetworkState() *NetworkState {
	state := &NetworkState{running: true, runChannel: make(chan bool)}
	state.tree = kdtree.NewKDTree([]kdtree.Point{})
	return state
}

// SetNetworkRunState updates the desired state of the networking
func (n *NetworkState) SetNetworkRunState(runState bool) {
	n.mux.Lock()
	defer n.mux.Unlock()
	if n.running != runState {
		n.running = runState
		go func() { n.runChannel <- runState }()
	}
}

// RefreshActiveNodes fetches the latest status of nodes in the pool
func (n *NetworkState) RefreshActiveNodes() {
	// Make a request to controld for the currently active nodes
	// todo: viper.GetString() for host config
	responseBytes, err := http.GetJSONBytes("http://localhost:3001/api/pool/0xDAcd582c3Ba1A90567Da0fC3f1dBB638D9438e06/nodes/approved")
	if err != nil {
		log.Fatal(err)
	}
	// Parse the 'response' field from the response data
	_nodes := gjson.GetBytes(responseBytes, "response").Array()

	// Create NetworkNode structs from the list of nodes returned from controld
	nodes := make([]kdtree.Point, 0)
	for i := 0; i < len(_nodes); i++ {
		ipStr := strings.TrimSpace(_nodes[i].Get("data.ip").String())
		ip := net.ParseIP(ipStr)
		if ip == nil {
			log.Printf("Invalid IP Address found in node: %v", _nodes[i])
			// continue
		}
		newNode := NewNetworkNode(0, 0, ip)
		nodes = append(nodes, newNode)
	}

	// Create a new KD-Tree with the new set of nodes
	newTree := kdtree.NewKDTree(nodes)
	n.mux.Lock()
	n.tree = newTree
	n.mux.Unlock()
}

// RunningStateChanged returns a channel that updates when the running state
// is changed
func (n *NetworkState) RunningStateChanged() chan (bool) {
	return n.runChannel
}

// NetworkNode is a struct representing an edge node in a pool
type NetworkNode struct {
	kdtree.Point // Implements Point interface

	longitude float64 // Longitude
	latitude  float64 // Latitude
	ip        net.IP
}

// NewNetworkNode returns a new NetworkNode struct
func NewNetworkNode(long, lat float64, ip net.IP) *NetworkNode {
	return &NetworkNode{longitude: long, latitude: lat, ip: ip}
}

func (n *NetworkNode) Dim() int {
	return 2
}

func (n *NetworkNode) GetValue(dim int) float64 {
	if dim == 0 {
		return n.longitude
	}
	return n.latitude
}

func (n *NetworkNode) Distance(other kdtree.Point) float64 {
	var ret float64
	for i := 0; i < n.Dim(); i++ {
		tmp := n.GetValue(i) - other.GetValue(i)
		ret += tmp * tmp
	}
	return ret
}

func (n *NetworkNode) PlaneDistance(val float64, dim int) float64 {
	tmp := n.GetValue(dim) - val
	return tmp * tmp
}
