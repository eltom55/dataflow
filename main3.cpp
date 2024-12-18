#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cctype>
#include <map>
#include <sstream>
#include <unordered_map>
#include <algorithm>
#include <set>

using namespace std;

// Trims the whitespace from front and back of a line
string trim(const string& str) {
    // This uses the find_first/last_not_of functions which takes an input of escape chars to know what to IGNORE
    // ' ': Space - ignore spaces so we keep the commands per line separated, we only take the front and back whitespace off
    // \t: Horizontal tab
    // \n: Newline
    // \r: Carriage return
    // \f: Form feed
    // \v: Vertical tab
    size_t first = str.find_first_not_of(" \t\n\r\f\v"); // find first char that isn't whitespace

    if (first == string::npos) // if it is all whitespace return an empty string
        return "";

    size_t last = str.find_last_not_of(" \t\n\r\f\v"); // find last char that isn't whitespace
    return str.substr(first, (last - first + 1));
}

void writeDotFile(const vector<string>& lines) {
    ofstream outputFile("main.dot");

    if (!outputFile.is_open()) {
        cerr << "Error: Could not open file for writing." << endl;
        return;
    }

    // start graph in DOT form
    outputFile << "digraph G {" << endl;

    for (const auto& line : lines) { //write lines
        outputFile << "    " << line << endl;
    }

    outputFile << "}" << endl; //end graph

    outputFile.close();
    cout << "DOT notation generated and written to main.dot" << endl;
}

// New structure to hold variable tracking information
struct DataFlowInfo {
    set<string> taintedVars;  // Variables that contain data from SOURCE
    bool hasFlow;             // Whether we've found a flow to SINK
};

bool checkDataFlow(const vector<string>& llvm_lines) {
    // First build CFG using existing parser data structures
    unordered_map<string, int> labels;
    map<string, vector<string>> edges;
    int node_count = 0;
    string label;

    // Build labels map (from your parser logic)
    for (const auto& line : llvm_lines) {
        if (line.find("define") != string::npos) continue;

        if (line.find(':') == string::npos) {
            label = "entry";
        } else {
            label = line.substr(0, line.find(':'));
        }
        labels[label] = node_count;
        node_count++;
        break;
    }

    for (const auto& line : llvm_lines) {
        if (line.find(':') != string::npos) {
            label = line.substr(0, line.find(':'));
            if (labels.count(label) == 0) {
                labels[label] = node_count;
                node_count++;
            }
        }
    }

    // Build edges map (from your parser logic)
    string current_block_label = "entry";
    for (const auto& line : llvm_lines) {
        string trimmedLine = trim(line);
        if (trimmedLine.find(':') != string::npos) {
            current_block_label = trimmedLine.substr(0, trimmedLine.find(':'));
        }
        else if (trimmedLine.find("br") != string::npos) {
            if (trimmedLine.find("i1") != string::npos) {
                size_t trueLabelPos = trimmedLine.find("label %") + 7;
                size_t spacePos = trimmedLine.find(", ", trueLabelPos);
                size_t falseLabelPos = trimmedLine.find("label %", trueLabelPos + 1) + 7;
                string trueLabel = trimmedLine.substr(trueLabelPos, spacePos - trueLabelPos);
                string falseLabel = trimmedLine.substr(falseLabelPos);
                falseLabel = trim(falseLabel);
                
                edges[current_block_label].push_back(trim(trueLabel));
                edges[current_block_label].push_back(trim(falseLabel));
            }
            else {
                size_t labelPos = trimmedLine.find("label %") + 7;
                string targetLabel = trim(trimmedLine.substr(labelPos));
                edges[current_block_label].push_back(targetLabel);
            }
        }
    }

    // Now do dataflow analysis with CFG awareness
    DataFlowInfo flowInfo;
    unordered_map<string, set<string>> blockTaintedVars;
    current_block_label = "entry";
    bool changed;

    // Iterate until fixed point
    do {
        changed = false;
        for (const auto& line : llvm_lines) {
            string trimmedLine = trim(line);
            
            // Update current block
            if (trimmedLine.find(':') != string::npos) {
                current_block_label = trimmedLine.substr(0, trimmedLine.find(':'));
                
                // Merge tainted vars from predecessor blocks
                for (const auto& [pred, successors] : edges) {
                    if (find(successors.begin(), successors.end(), current_block_label) != successors.end()) {
                        for (const auto& var : blockTaintedVars[pred]) {
                            if (blockTaintedVars[current_block_label].insert(var).second) {
                                changed = true;
                            }
                            flowInfo.taintedVars.insert(var);
                        }
                    }
                }
                continue;
            }

            // Skip empty lines after trimming
            if (trimmedLine.empty()) continue;

            // Skip function declarations
            if (trimmedLine.find("define") != string::npos || 
                trimmedLine.find("declare") != string::npos) {
                continue;
            }

            // Check for SOURCE calls
            if (trimmedLine.find("@SOURCE") != string::npos) {
                size_t equalPos = trimmedLine.find('=');
                if (equalPos != string::npos) {
                    string resultVar = trim(trimmedLine.substr(0, equalPos));
                    flowInfo.taintedVars.insert(resultVar);
                    cout << "DEBUG: Tainted from SOURCE: " << resultVar << endl;
                }
                continue;
            }

            // Track store instructions
            if (trimmedLine.find("store") != string::npos) {
                size_t comma = trimmedLine.find(',');
                if (comma != string::npos) {
                    string fullSourceVal = trimmedLine.substr(trimmedLine.find("store") + 5, comma - (trimmedLine.find("store") + 5));
                    string destPtr = trimmedLine.substr(comma + 1);
                    
                    // Extract the actual value being stored (after the type)
                    string sourceVal;
                    size_t lastSpace = fullSourceVal.find_last_of(' ');
                    if (lastSpace != string::npos) {
                        sourceVal = trim(fullSourceVal.substr(lastSpace + 1));
                    } else {
                        sourceVal = trim(fullSourceVal);
                    }
                    destPtr = trim(destPtr);
                    // Remove comments if present
                    size_t commentPos = destPtr.find(';');
                    if (commentPos != string::npos) {
                        destPtr = trim(destPtr.substr(0, commentPos));
                    }
                    // Remove type information
                    if (destPtr.find(' ') != string::npos) {
                        destPtr = destPtr.substr(destPtr.find(' ') + 1);
                    }

                    // Check if storing a constant number
                    bool isConstant = true;
                    if (sourceVal[0] == '%') {
                        isConstant = false;
                    } else {
                        for (char c : sourceVal) {
                            if (!isdigit(c) && c != '-' && c != '+') {
                                isConstant = false;
                                break;
                            }
                        }
                    }

                    cout << "DEBUG: Store instruction - Source: " << sourceVal 
                         << ", Dest: " << destPtr 
                         << ", isConstant: " << (isConstant ? "true" : "false") << endl;

                    if (isConstant) {
                        flowInfo.taintedVars.erase(destPtr);
                        cout << "DEBUG: Untainted due to constant: " << destPtr << endl;
                    } else if (flowInfo.taintedVars.find(sourceVal) != flowInfo.taintedVars.end()) {
                        flowInfo.taintedVars.insert(destPtr);
                        cout << "DEBUG: Tainted from store: " << destPtr << endl;
                    }
                }
            }

            // Track load instructions
            if (trimmedLine.find("load") != string::npos) {
                size_t equalPos = trimmedLine.find('=');
                if (equalPos != string::npos) {
                    string resultVar = trim(trimmedLine.substr(0, equalPos));
                    string sourcePtr = trimmedLine.substr(trimmedLine.find("ptr") + 3);
                    sourcePtr = trim(sourcePtr);

                    cout << "DEBUG: Load instruction - Result: " << resultVar 
                         << ", Source: " << sourcePtr << endl;

                    if (flowInfo.taintedVars.find(sourcePtr) != flowInfo.taintedVars.end()) {
                        flowInfo.taintedVars.insert(resultVar);
                        cout << "DEBUG: Tainted from load: " << resultVar << endl;
                    }
                }
            }

            // Check for SINK calls
            if (trimmedLine.find("@SINK") != string::npos) {
                size_t openParen = trimmedLine.find('(');
                size_t closeParen = trimmedLine.find(')');
                if (openParen != string::npos && closeParen != string::npos) {
                    string sinkArg = trimmedLine.substr(openParen + 1, closeParen - openParen - 1);
                    sinkArg = trim(sinkArg);
                    if (sinkArg.find(' ') != string::npos) {
                        sinkArg = sinkArg.substr(sinkArg.find(' ') + 1);
                    }
                    cout << "DEBUG: SINK called with: " << sinkArg 
                         << " (tainted: " << (flowInfo.taintedVars.find(sinkArg) != flowInfo.taintedVars.end() ? "yes" : "no") << ")" << endl;
                    
                    if (flowInfo.taintedVars.find(sinkArg) != flowInfo.taintedVars.end()) {
                        return true;
                    }
                }
            }

            // Update block-specific tainted vars
            for (const auto& var : flowInfo.taintedVars) {
                if (blockTaintedVars[current_block_label].insert(var).second) {
                    changed = true;
                }
            }
        }
    } while (changed);

    return false;
}

void parseLLVMtoDOT(const vector<string>& llvm_lines) {
  vector<string> parsed_lines;
  unordered_map<string, int> labels;
  map<string, vector<string>> edges;
  int node_count = 0;

  // add label to hash of labels with values of label number (0,1,2...)
  string label;

  // Grab first block label or init to "entry"
  // This only handles the first few lines to decide if there is an entry block defined or if it just immediately starts doing code
  for (const auto& line : llvm_lines) {
    if (line.find("define") != string::npos) { // if define is in the string its a function header
      continue; // skip the function header
    }

    if (line.find(':') == string::npos) { // if : not in string
      label = "entry";
    } else {
      label = line.substr(0, line.find(':')); // otherwise grab the label name
    }

    labels[label] = node_count; // label_name: node_number in labels hash
    node_count++;
    break;
  }

  // parse for the rest of the labels
  for (const auto& line : llvm_lines) {
    if (line.find(':') != string::npos) { // checking if it is a label
      label = line.substr(0, line.find(':')); //grab the label name
      if (labels.count(label) == 0) { // add label to labels hash if not already a key
        labels[label] = node_count;
        node_count++;
      }
    }
  }

  // cout << "\n\nLabels Hash:" << endl;
  // for (const auto& kv : labels) {
  //   cout << kv.first << ": " << kv.second << endl;
  // }

  ostringstream oss; // this is used to concat stuff later, but i think we can just use + to concat on a refactor, i was just dumb

  // parse for edges
  string current_block_label = "entry";
  for (const auto& line : llvm_lines) {
    if (line.find(':') != string::npos) { // if we hit a new label line
      current_block_label = line.substr(0, line.find(':')); //update current label
      // cout << "new current_block_label: ||||||||" << current_block_label << "|||||||" << endl;
    }
    else if (line.find("br") != string::npos) { // if we hit a jump
      size_t secondItemPos = line.find(' ') + 1;
      size_t spaceAfterItemPos = line.find(' ', secondItemPos) - 3; // idk why i have to - 3 here but it works, im not quite using find correctly
      string item = line.substr(secondItemPos, spaceAfterItemPos); // either i1 or label, see examples below
      string edge_notation;

      // cout << "item: " << item << endl;
      if (item == "i1") { // Ex. br i1 %noArgs, label %lbl_t, label %lbl_f
        string trueLabel, falseLabel;
        size_t trueLabelPos = line.find("label %") + 7;
        size_t spacePos = line.find(", ", trueLabelPos);
        size_t falseLabelPos = line.find("label %", trueLabelPos + 1) + 7;
        trueLabel = line.substr(trueLabelPos, spacePos - trueLabelPos);
        falseLabel = line.substr(falseLabelPos);
        // cout << "true/false: " << trueLabel << " / " << falseLabel << endl;

        oss.str(""); // this is where i think we can get rid of oss and just concat with +
        oss.clear();
        oss << "Node" << labels[current_block_label] << " -> Node" << labels[trueLabel] << " [label=0];";
        edges[current_block_label].push_back(oss.str());
        // cout << "push true" << endl;

        oss.str("");
        oss.clear();
        oss << "Node" << labels[current_block_label] << " -> Node" << labels[falseLabel] << " [label=1];";
        edges[current_block_label].push_back(oss.str()); // housing label: full DOT notation edge string in edges hash
        // cout << "push false" << endl;



      }
      else if (item == "label") { // Ex. br label %myLabel
        size_t labelPos = line.find("label %") + 7;
        string label = line.substr(labelPos);

        oss.str("");
        oss.clear();
        oss << "Node" << labels[current_block_label] << " -> Node" << labels[label] << " [label=0];";
        edges[current_block_label].push_back(oss.str()); // housing label: full DOT notation edge string in edges hash
        // cout << "push default" << endl;

      }
    }
  }

  // cout << "Printing labels" << endl;
  // for (const auto& pair : labels) {
  //   cout << pair.first << ": " << pair.second << endl;
  // }

  // cout << "Printing edges" << endl;
  // for (const auto& pair : edges) {
  //   cout << pair.first << ": \n"; // label
  //   for (const auto& edge : pair.second) { //for edge in edge list
  //     cout << edge << " ";
  //   }
  //   cout << endl; // New line after printing each key's edges
  // }

  vector<pair<int, string>> labels_hash_reversed; // for some reason the labels hash is in descending order, so gotta reverse to easily write to dot file

  // Reverse the unordered_map
  for (const auto& kv : labels) {
      labels_hash_reversed.push_back(make_pair(kv.second, kv.first)); // pair is (value, key) so we can sort by begin and end
  }
  sort(labels_hash_reversed.begin(), labels_hash_reversed.end()); // Sort by value (the first element of pair)

  // Print the reversed pairs
  // for (const auto& pair : labels_hash_reversed) {
  //     cout << "Key: " << pair.second << ", Value: " << pair.first << endl;
  // }

  for (const auto& kv_pair : labels_hash_reversed) {
    string label = kv_pair.second;
    int index = kv_pair.first;

    oss.str("");
    oss.clear();
    oss << "Node" << index << " [shape=record,label=\""<< label <<"\"]";
    parsed_lines.push_back(oss.str()); // push label header
    if (edges.count(label) > 0) { // Block has edges
      // cout << label << " has edges" << endl;
      for (const auto& edge : edges[label]) { // for every edge associated with this node
        parsed_lines.push_back(edge);
      }
    }
  }

  // cout << "\n\n\n\n\n\n\n\n";
  // for (const auto& line : parsed_lines) {
  //   cout << line << endl;
  // }

  writeDotFile(parsed_lines);
}

// Modify main to include the dataflow analysis
int main(int argc, char* argv[]) {
    if (argc != 2) {
        cerr << "Usage: ./graph <input_file.ll>" << endl;
        return 1;
    }

    string inputFilename = argv[1];

    ifstream inputFile(inputFilename);
    if (!inputFile.is_open()) {
        cerr << "Error opening file: " << inputFilename << endl;
        return 1;
    }

    vector<string> lines;
    string line;
    while (getline(inputFile, line)) {
        lines.push_back(trim(line));
    }

    inputFile.close();

    // Only perform dataflow analysis
    bool hasFlow = checkDataFlow(lines);
    cout << (hasFlow ? "FLOW" : "NO FLOW") << endl;

    return 0;
}