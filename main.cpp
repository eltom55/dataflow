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

// First, let's add a structure to track dataflow information in each node
struct Node {
    string label;
    vector<string> instructions;
    set<string> tainted_vars;
    vector<int> successors;  // Node numbers of successor blocks
};

// Modify parseLLVMtoDOT to return the graph structure
vector<Node> parseLLVMtoDOT(const vector<string>& llvm_lines) {
    vector<Node> nodes;
    unordered_map<string, int> labels;
    Node current_node;
    bool in_function = false;

    // First pass: collect all labels
    for (const auto& line : llvm_lines) {
        if (line.find("define") != string::npos) {
            in_function = true;
            current_node.label = "entry";
            continue;
        }

        if (!in_function) continue;

        if (line.find(':') != string::npos) {
            // If we have a current node, save it
            if (!current_node.label.empty()) {
                nodes.push_back(current_node);
                current_node = Node();
            }
            current_node.label = line.substr(0, line.find(':'));
        } else if (!line.empty()) {
            current_node.instructions.push_back(line);
        }
    }

    // Don't forget to add the last node
    if (!current_node.label.empty()) {
        nodes.push_back(current_node);
    }

    // Debug output
    cout << "Parsed " << nodes.size() << " nodes:" << endl;
    for (const auto& node : nodes) {
        cout << "Node " << node.label << " has " << node.instructions.size() << " instructions" << endl;
    }

    // Generate DOT file (keep your existing DOT generation code)
    vector<string> dot_lines;
    // Add your DOT format lines here
    writeDotFile(dot_lines);

    return nodes;
}

// Modified hasDataFlow to use the graph structure
bool hasDataFlow(vector<Node>& nodes) {
    set<string> tainted;
    bool flowFound = false;
    
    cout << "\nDetailed Taint Analysis:" << endl;
    cout << "------------------------" << endl;
    
    // Process each node in the graph
    for (auto& node : nodes) {
        cout << "\nBlock: " << node.label << endl;
        cout << "------------------------" << endl;
        
        for (const auto& inst : node.instructions) {
            cout << "Analyzing: " << inst << endl;
            
            // SOURCE handling
            if (inst.find("@SOURCE") != string::npos || inst.find("@source") != string::npos) {  // Case insensitive
                size_t equalPos = inst.find("=");
                size_t varPos = inst.find("%");
                if (varPos != string::npos) {
                    string var = inst.substr(varPos);
                    size_t endPos = var.find_first_of(" ,)");
                    if (endPos != string::npos) {
                        var = var.substr(0, endPos);
                    }
                    var = trim(var);
                    cout << "SOURCE found -> Tainting: " << var << endl;
                    tainted.insert(var);
                }
            }
            
            // Store handling
            else if (inst.find("store") != string::npos) {
                size_t firstVarPos = inst.find("%");
                size_t commaPos = inst.find(",");
                
                // Check if we're storing a constant (like "store i32 0")
                bool isConstantStore = inst.find("store i32 0") != string::npos || 
                                      inst.find("store i32 1") != string::npos ||
                                      (inst.find("store i32") != string::npos && firstVarPos > commaPos);
                
                if (isConstantStore) {
                    // Find the destination variable
                    size_t destVarPos = inst.find("%", commaPos);
                    if (destVarPos != string::npos) {
                        string destVar = inst.substr(destVarPos);
                        size_t endPos = destVar.find_first_of(" ,)");
                        if (endPos != string::npos) {
                            destVar = destVar.substr(0, endPos);
                        }
                        destVar = trim(destVar);
                        cout << "Constant store to " << destVar << " - removing taint" << endl;
                        tainted.erase(destVar);
                    }
                }
                else if (firstVarPos != string::npos && commaPos != string::npos) {
                    string sourceVar = inst.substr(firstVarPos, commaPos - firstVarPos);
                    sourceVar = trim(sourceVar);
                    
                    size_t secondVarPos = inst.find("%", commaPos);
                    if (secondVarPos != string::npos) {
                        string destVar = inst.substr(secondVarPos);
                        destVar = trim(destVar);
                        
                        cout << "Store instruction: " << sourceVar << " -> " << destVar << endl;
                        if (tainted.find(sourceVar) != tainted.end()) {
                            cout << "  Propagating taint: " << sourceVar << " -> " << destVar << endl;
                            tainted.insert(destVar);
                        } else {
                            cout << "  Source not tainted: " << sourceVar << endl;
                            tainted.erase(destVar);  // Clear taint if source is not tainted
                        }
                    }
                }
            }
            
            // Load handling
            else if (inst.find("load") != string::npos) {
                size_t destPos = inst.find("%");
                size_t equalPos = inst.find("=");
                size_t sourcePos = inst.find("%", equalPos);
                if (destPos != string::npos && equalPos != string::npos && sourcePos != string::npos) {
                    string destVar = inst.substr(destPos, equalPos - destPos);
                    string sourceVar = inst.substr(sourcePos);
                    destVar = trim(destVar);
                    sourceVar = trim(sourceVar);
                    
                    cout << "Load instruction: " << sourceVar << " -> " << destVar << endl;
                    if (tainted.find(sourceVar) != tainted.end()) {
                        cout << "  Propagating taint: " << sourceVar << " -> " << destVar << endl;
                        tainted.insert(destVar);
                    } else {
                        cout << "  Source not tainted: " << sourceVar << endl;
                    }
                }
            }
            
            // SINK handling
            else if (inst.find("@SINK") != string::npos || inst.find("@sink") != string::npos) {  // Case insensitive
                size_t varPos = inst.find("%");
                if (varPos != string::npos) {
                    string var = inst.substr(varPos);
                    size_t endPos = var.find_first_of(" ,)");
                    if (endPos != string::npos) {
                        var = var.substr(0, endPos);
                    }
                    var = trim(var);
                    
                    cout << "SINK found with variable: " << var << endl;
                    cout << "Current tainted set: ";
                    for (const auto& t : tainted) {
                        cout << t << " ";
                    }
                    cout << endl;
                    
                    if (tainted.find(var) != tainted.end()) {
                        cout << "!!! TAINTED FLOW DETECTED !!!" << endl;
                        flowFound = true;
                    } else {
                        cout << "  Variable not tainted: " << var << endl;
                    }
                }
            }
        }
        
        cout << "\nEnd of block tainted variables: ";
        for (const auto& var : tainted) {
            cout << var << " ";
        }
        cout << "\n" << endl;
    }
    
    return flowFound;
}

// Main function
int main(int argc, char* argv[]) {
    if (argc != 2) {
        cerr << "Usage: ./graph <input_file.ll>" << endl;
        return 1;
    }

    string inputFilename = argv[1];

    ifstream inputFile(inputFilename);
    if (!inputFile.is_open())
    {
        cerr << "Error opening file: " << inputFilename << endl;
        return 1;
    }

    vector<string> lines;
    string line;
    while (getline(inputFile, line))
    {
        lines.push_back(trim(line));
    }

    inputFile.close();

    // Parse the LLVM IR and build the graph
    vector<Node> graph = parseLLVMtoDOT(lines);
    
    // Perform dataflow analysis on the graph
    if (hasDataFlow(graph)) {
        cout << "FLOW" << endl;
    } else {
        cout << "NO FLOW" << endl;
    }
    
    return 0;
}