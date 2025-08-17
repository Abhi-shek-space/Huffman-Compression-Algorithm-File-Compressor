#include <iostream>
#include <fstream>
#include <queue>
#include <unordered_map>
#include <vector>
#include <bitset>
#include <string>
#include <memory>
#include <cstring>
#include <iomanip>

using namespace std;

// Node structure for Huffman Tree
struct HuffmanNode {
    unsigned char data;
    unsigned freq;
    shared_ptr<HuffmanNode> left, right;
    
    HuffmanNode(unsigned char d, unsigned f) : data(d), freq(f), left(nullptr), right(nullptr) {}
};

// Comparator for priority queue (min-heap based on frequency)
struct CompareNode {
    bool operator()(const shared_ptr<HuffmanNode>& a, const shared_ptr<HuffmanNode>& b) {
        return a->freq > b->freq;
    }
};

class HuffmanCompressor {
private:
    unordered_map<unsigned char, string> huffmanCodes;
    unordered_map<string, unsigned char> reverseHuffmanCodes;
    shared_ptr<HuffmanNode> root;
    
    // Calculate frequency of each byte in the file
    unordered_map<unsigned char, unsigned> calculateFrequency(const string& filename) {
        unordered_map<unsigned char, unsigned> freq;
        ifstream file(filename, ios::binary);
        
        if (!file) {
            throw runtime_error("Cannot open file: " + filename);
        }
        
        unsigned char ch;
        while (file.read(reinterpret_cast<char*>(&ch), 1)) {
            freq[ch]++;
        }
        
        file.close();
        return freq;
    }
    
    // Build Huffman Tree using priority queue
    shared_ptr<HuffmanNode> buildHuffmanTree(const unordered_map<unsigned char, unsigned>& freq) {
        priority_queue<shared_ptr<HuffmanNode>, 
                      vector<shared_ptr<HuffmanNode>>, 
                      CompareNode> pq;
        
        // Create leaf nodes for each character
        for (const auto& pair : freq) {
            pq.push(make_shared<HuffmanNode>(pair.first, pair.second));
        }
        
        // Build tree bottom-up
        while (pq.size() > 1) {
            auto left = pq.top(); pq.pop();
            auto right = pq.top(); pq.pop();
            
            auto parent = make_shared<HuffmanNode>('\0', left->freq + right->freq);
            parent->left = left;
            parent->right = right;
            
            pq.push(parent);
        }
        
        return pq.top();
    }
    
    // Generate Huffman codes for each character
    void generateCodes(const shared_ptr<HuffmanNode>& node, const string& code) {
        if (!node) return;
        
        if (!node->left && !node->right) {
            huffmanCodes[node->data] = code.empty() ? "0" : code;
            reverseHuffmanCodes[code.empty() ? "0" : code] = node->data;
            return;
        }
        
        generateCodes(node->left, code + "0");
        generateCodes(node->right, code + "1");
    }
    
    // Write compressed data to file with header
    void writeCompressedFile(const string& inputFile, const string& outputFile) {
        ifstream input(inputFile, ios::binary);
        ofstream output(outputFile, ios::binary);
        
        if (!input || !output) {
            throw runtime_error("Cannot open files for compression");
        }
        
        // Write header (frequency table)
        unsigned mapSize = huffmanCodes.size();
        output.write(reinterpret_cast<const char*>(&mapSize), sizeof(mapSize));
        
        for (const auto& pair : huffmanCodes) {
            output.write(reinterpret_cast<const char*>(&pair.first), sizeof(unsigned char));
            unsigned codeLen = pair.second.length();
            output.write(reinterpret_cast<const char*>(&codeLen), sizeof(codeLen));
            output.write(pair.second.c_str(), codeLen);
        }
        
        // Encode and write compressed data
        string buffer;
        unsigned char ch;
        unsigned long long totalBits = 0;
        
        while (input.read(reinterpret_cast<char*>(&ch), 1)) {
            buffer += huffmanCodes[ch];
            totalBits += huffmanCodes[ch].length();
            
            // Write complete bytes
            while (buffer.length() >= 8) {
                unsigned char byte = 0;
                for (int i = 0; i < 8; i++) {
                    if (buffer[i] == '1') {
                        byte |= (1 << (7 - i));
                    }
                }
                output.write(reinterpret_cast<const char*>(&byte), 1);
                buffer = buffer.substr(8);
            }
        }
        
        // Write remaining bits with padding info
        if (!buffer.empty()) {
            unsigned char byte = 0;
            for (size_t i = 0; i < buffer.length(); i++) {
                if (buffer[i] == '1') {
                    byte |= (1 << (7 - i));
                }
            }
            output.write(reinterpret_cast<const char*>(&byte), 1);
        }
        
        // Write padding info
        unsigned char padding = buffer.empty() ? 0 : 8 - buffer.length();
        output.write(reinterpret_cast<const char*>(&padding), 1);
        
        input.close();
        output.close();
    }
    
public:
    // Compress file using Huffman encoding
    void compressFile(const string& inputFile, const string& outputFile) {
        cout << "Starting compression...\n";
        
        // Step 1: Calculate frequency
        auto freq = calculateFrequency(inputFile);
        cout << "Frequency analysis complete. Found " << freq.size() << " unique bytes.\n";
        
        // Step 2: Build Huffman tree
        root = buildHuffmanTree(freq);
        
        // Step 3: Generate codes
        huffmanCodes.clear();
        reverseHuffmanCodes.clear();
        generateCodes(root, "");
        
        // Step 4: Write compressed file
        writeCompressedFile(inputFile, outputFile);
        
        // Calculate compression ratio
        ifstream input(inputFile, ios::binary | ios::ate);
        ifstream output(outputFile, ios::binary | ios::ate);
        
        long long inputSize = input.tellg();
        long long outputSize = output.tellg();
        
        double ratio = (1.0 - (double)outputSize / inputSize) * 100;
        
        cout << "\nCompression complete!\n";
        cout << "Original size: " << inputSize << " bytes\n";
        cout << "Compressed size: " << outputSize << " bytes\n";
        cout << "Compression ratio: " << fixed << setprecision(2) << ratio << "%\n";
        
        input.close();
        output.close();
    }
    
    // Decompress file
    void decompressFile(const string& inputFile, const string& outputFile) {
        cout << "Starting decompression...\n";
        
        ifstream input(inputFile, ios::binary);
        ofstream output(outputFile, ios::binary);
        
        if (!input || !output) {
            throw runtime_error("Cannot open files for decompression");
        }
        
        // Read header
        unsigned mapSize;
        input.read(reinterpret_cast<char*>(&mapSize), sizeof(mapSize));
        
        reverseHuffmanCodes.clear();
        for (unsigned i = 0; i < mapSize; i++) {
            unsigned char ch;
            unsigned codeLen;
            input.read(reinterpret_cast<char*>(&ch), sizeof(unsigned char));
            input.read(reinterpret_cast<char*>(&codeLen), sizeof(codeLen));
            
            string code(codeLen, ' ');
            input.read(&code[0], codeLen);
            reverseHuffmanCodes[code] = ch;
        }
        
        // Read compressed data
        vector<unsigned char> compressedData;
        unsigned char byte;
        while (input.read(reinterpret_cast<char*>(&byte), 1)) {
            compressedData.push_back(byte);
        }
        
        // Get padding info (last byte)
        unsigned char padding = compressedData.back();
        compressedData.pop_back();
        
        // Decode
        string currentCode;
        for (size_t i = 0; i < compressedData.size(); i++) {
            unsigned char byte = compressedData[i];
            int bitsToProcess = (i == compressedData.size() - 1) ? (8 - padding) : 8;
            
            for (int j = 0; j < bitsToProcess; j++) {
                currentCode += ((byte & (1 << (7 - j))) ? '1' : '0');
                
                if (reverseHuffmanCodes.find(currentCode) != reverseHuffmanCodes.end()) {
                    output.write(reinterpret_cast<const char*>(&reverseHuffmanCodes[currentCode]), 1);
                    currentCode = "";
                }
            }
        }
        
        input.close();
        output.close();
        
        cout << "Decompression complete!\n";
    }
    
    // Display Huffman codes for analysis
    void displayHuffmanCodes() {
        cout << "\nHuffman Codes:\n";
        cout << "Character  | Frequency | Code\n";
        cout << "-----------|-----------|-----\n";
        
        for (const auto& pair : huffmanCodes) {
            if (isprint(pair.first)) {
                cout << "    '" << pair.first << "'   ";
            } else {
                cout << "   0x " << hex << setw(2) << setfill('0') 
                     << (int)pair.first << dec << "  ";
            }
            cout << " |           | " << pair.second << "\n";
        }
    }
};

// Test function to verify compression/decompression
bool verifyCompression(const string& originalFile, const string& decompressedFile) {
    ifstream file1(originalFile, ios::binary);
    ifstream file2(decompressedFile, ios::binary);
    
    if (!file1 || !file2) return false;
    
    char ch1, ch2;
    while (file1.get(ch1) && file2.get(ch2)) {
        if (ch1 != ch2) return false;
    }
    
    return !file1.get(ch1) && !file2.get(ch2);
}

int main() {
    HuffmanCompressor compressor;
    
    cout << "=== Huffman Compression Tool ===\n\n";
    
    int choice;
    string inputFile, outputFile;
    
    while (true) {
        cout << "\nMenu:\n";
        cout << "1. Compress file\n";
        cout << "2. Decompress file\n";
        cout << "3. Test with sample text file\n";
        cout << "4. Exit\n";
        cout << "Enter choice: ";
        cin >> choice;
        
        switch (choice) {
            case 1:
                cout << "Enter input filename: ";
                cin >> inputFile;
                cout << "Enter output filename (compressed): ";
                cin >> outputFile;
                
                try {
                    compressor.compressFile(inputFile, outputFile);
                    compressor.displayHuffmanCodes();
                } catch (const exception& e) {
                    cerr << "Error: " << e.what() << "\n";
                }
                break;
                
            case 2:
                cout << "Enter compressed filename: ";
                cin >> inputFile;
                cout << "Enter output filename (decompressed): ";
                cin >> outputFile;
                
                try {
                    compressor.decompressFile(inputFile, outputFile);
                } catch (const exception& e) {
                    cerr << "Error: " << e.what() << "\n";
                }
                break;
                
            case 3: {
                // Create a sample text file for testing
                string testFile = "test_input.txt";
                string compressedFile = "test_compressed.huf";
                string decompressedFile = "test_decompressed.txt";
                
                ofstream test(testFile);
                test << "This is a test file for Huffman compression algorithm.\n";
                test << "Huffman coding is a lossless data compression algorithm.\n";
                test << "The algorithm uses a variable-length code table for encoding.\n";
                test << "Characters that appear more frequently get shorter codes.\n";
                test << "This results in optimal prefix codes and efficient compression!\n";
                test.close();
                
                cout << "\nTesting compression and decompression...\n";
                
                try {
                    compressor.compressFile(testFile, compressedFile);
                    compressor.displayHuffmanCodes();
                    compressor.decompressFile(compressedFile, decompressedFile);
                    
                    if (verifyCompression(testFile, decompressedFile)) {
                        cout << "\n✓ Verification successful! Files match perfectly.\n";
                    } else {
                        cout << "\n✗ Verification failed! Files don't match.\n";
                    }
                } catch (const exception& e) {
                    cerr << "Error: " << e.what() << "\n";
                }
                break;
            }
                
            case 4:
                cout << "Exiting...\n";
                return 0;
                
            default:
                cout << "Invalid choice!\n";
        }
    }
    
    return 0;
}