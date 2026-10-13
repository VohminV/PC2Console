#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "pc2c/analysis/Inventory.h"
#include "pc2c/core/Config.h"
#include "pc2c/core/Json.h"
#include "pc2c/discovery/Scanner.h"

namespace pc2c::graph {

struct Node {
  std::string rel;
  std::string kind;
  std::string format;
};

struct Edge {
  std::string from;
  std::string to;      // resolved rel, or raw token when unresolved
  std::string basis;   // "exec" | "ref-ext" | "datablock"
  int line = 0;
  bool resolved = false;
};

struct Graph {
  std::vector<Node> nodes;
  std::vector<Edge> edges;
  std::size_t filesScanned = 0;
  std::size_t textFiles = 0;
  std::size_t resolved = 0;
  std::size_t unresolved = 0;
  std::vector<std::string> warnings;
};

// Build a file/resource dependency graph from text references.
// Inputs: scan + inventory (no execution). Text sources (.cs/.gui/.mis)
// are read bounded (2MB) and parsed by the Torque adapter; every other
// file becomes a node without outgoing edges. Unknown targets stay as
// unresolved edges — never faked as resolved.
Graph buildGraph(const std::string& root, const discovery::ScanResult& scan,
                 const analysis::Inventory& inv, const Limits& limits);

json::Value graphToJson(const Graph& g);

}  // namespace pc2c::graph
