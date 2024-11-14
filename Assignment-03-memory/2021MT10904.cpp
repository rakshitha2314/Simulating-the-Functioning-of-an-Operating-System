#include <iostream>
#include <unordered_map>

using namespace std;

// Custom queue implementation (used for FIFO and LIFO)
class Queue {
private:
    int front, rear, size;
    int capacity;
    int* array;

public:
    // Constructor to initialize queue
    Queue(int capacity) {
        this->capacity = capacity;
        front = 0;
        rear = -1;
        size = 0;
        array = new int[capacity];
    }

    // Destructor to free memory
    ~Queue() {
        delete[] array;
    }

    // Check if queue is full
    bool isFull() const {
        return size == capacity;
    }

    // Check if queue is empty
    bool isEmpty() const {
        return size == 0;
    }

    // Add page number to the queue (FIFO)
    void enqueue(int pageNumber) {
        if (isFull()) return; // Prevent enqueue if full
        rear = (rear + 1) % capacity;
        array[rear] = pageNumber;
        size++;
    }

    // Remove and return the front page number (FIFO)
    int dequeue() {
        if (isEmpty()) return -1; // Return -1 if empty
        int pageNumber = array[front];
        front = (front + 1) % capacity;
        size--;
        return pageNumber;
    }

    // Remove and return the element from the rear of the queue (for LIFO)
    int dequeueRear() {
        if (isEmpty()) return -1; // Return -1 if empty
        int pageNumber = array[rear];
        rear = (rear - 1 + capacity) % capacity;
        size--;
        return pageNumber;
    }
};


// Node structure for doubly linked list
struct Node {
    int pageNumber;
    Node* prev;
    Node* next;
    Node(int page) : pageNumber(page), prev(nullptr), next(nullptr) {}
};


// FIFO TLB replacement algorithm
int FIFO(int S, int P, int K, int N, unsigned int addresses[]) {
    unordered_map<int, bool> tlbMap; // TLB map to track page numbers
    Queue tlbQueue(K);               // Queue to handle FIFO
    int tlbHits = 0;

    for (int i = 0; i < N; i++) {
        unsigned int address = addresses[i];
        //cout<<address<<" ";
        int pageNumber = address / (P * 1024); // Calculate page number
        //cout<<pageNumber<<" ";
        if (tlbMap.count(pageNumber)) { // Check if the page is in the TLB
            tlbHits++; // TLB hit
        } else {
            // TLB miss
            if (tlbQueue.isFull()) {
                int oldPage = tlbQueue.dequeue(); // Remove oldest page
                tlbMap.erase(oldPage);            // Erase from map
            }
            tlbQueue.enqueue(pageNumber);         // Add new page to queue
            tlbMap[pageNumber] = true;            // Mark as present in TLB
        }
    }

    return tlbHits;
}

// LIFO TLB replacement algorithm
int LIFO(int S, int P, int K, int N, unsigned int addresses[]) {
    unordered_map<int, bool> tlbMap; // TLB map to track page numbers
    Queue tlbQueue(K);               // Queue to handle LIFO
    int tlbHits = 0;

    for (int i = 0; i < N; i++) {
        unsigned int address = addresses[i];
        int pageNumber = address / (P * 1024); // Calculate page number

        if (tlbMap.count(pageNumber)) { // Check if the page is in the TLB
            tlbHits++; // TLB hit
        } else {
            // TLB miss
            if (tlbQueue.isFull()) {
                int lastPage = tlbQueue.dequeueRear(); // Remove last added page (LIFO)
                tlbMap.erase(lastPage);               // Erase from map
            }
            tlbQueue.enqueue(pageNumber);             // Add new page
            tlbMap[pageNumber] = true;                // Mark as present in TLB
        }
    }

    return tlbHits;
}

// LRU TLB replacement algorithm
// LRU TLB replacement algorithm
int LRU(int S, int P, int K, int N, unsigned int addresses[]) {
    unordered_map<int, Node*> tlbMap;  // Maps pageNumber to its node in the list
    Node* head = nullptr;              // Head of the doubly linked list
    Node* tail = nullptr;              // Tail of the doubly linked list
    int tlbHits = 0;

    for (int i = 0; i < N; i++) {
        unsigned int address = addresses[i];
        int pageNumber = address / (P * 1024); // Calculate page number

        if (tlbMap.count(pageNumber)) { // Check if the page is in the TLB
            tlbHits++; // TLB hit

            // Move the accessed page to the front (most recently used)
            Node* node = tlbMap[pageNumber];

            // If the node is already at the front, no need to move it
            if (node != head) {
                // Remove node from its current position in the list
                if (node->prev) {
                    node->prev->next = node->next;
                }
                if (node->next) {
                    node->next->prev = node->prev;
                }

                // If the node is the tail, update the tail pointer
                if (node == tail) {
                    tail = node->prev;
                }

                // Move the node to the front
                node->next = head;
                node->prev = nullptr;

                if (head) {
                    head->prev = node;  // Update the old head's previous pointer
                }
                head = node;

                // If the list only had one node, update the tail
                if (!tail) {
                    tail = head;
                }
            }
        } else {
            // TLB miss
            if (tlbMap.size() == K) {
                // TLB is full: remove the least recently used page (tail)
                int lruPage = tail->pageNumber;
                tlbMap.erase(lruPage); // Remove from the map

                // Remove the tail node from the linked list
                Node* oldTail = tail;
                tail = tail->prev;
                if (tail) {
                    tail->next = nullptr;  // Update the new tail
                } else {
                    head = nullptr;  // If the list is now empty
                }
                delete oldTail;  // Free the memory of the old tail node
            }

            // Add new page to the TLB
            Node* newNode = new Node(pageNumber);

            // Add it to the front of the list
            newNode->next = head;
            newNode->prev = nullptr;
            if (head) {
                head->prev = newNode;  // Update the previous head
            }
            head = newNode;

            // If the list was empty, the new node is both head and tail
            if (!tail) {
                tail = newNode;
            }

            tlbMap[pageNumber] = newNode;  // Add the new node to the map
        }
    }

    // Clean up the linked list nodes after processing all addresses
    while (head) {
        Node* temp = head;
        head = head->next;
        delete temp;
    }

    return tlbHits;
}

// Optimal TLB replacement algorithm
int Optimal(int S, int P, int K, int N, unsigned int addresses[]) 
{
    unordered_map<int, int> tlbMap; // TLB map to track page numbers
    unordered_map<int, int> nextUse; // Tracks next use index of each page
    unordered_map<int, int> page_map; // Tracks next use index of each page     //changes made
    int tlbHits = 0; 
    // Initialize next use for all pages to -1
    for (int i = 0; i < N; i++) 
    {
        int pageNumber=addresses[i] / (P * 1024);
        page_map[pageNumber] = -1; // Initialize all pages // changes here
        nextUse[i]=-1;
    }

    // Fill the next use indices
    for (int i = N - 1; i >= 0; i--) 
    {
        int pageNumber = addresses[i] / (P * 1024);
        nextUse[i]=page_map[pageNumber];  // changes here
        page_map[pageNumber] = i; // Update next use index // changes here
    }

    for (int i = 0; i < N; i++) 
    {
        int pageNumber = addresses[i] / (P * 1024);

        if (tlbMap.count(pageNumber)) {
            tlbMap[pageNumber] = i; // Check if the page is in the TLB  // changes here
            tlbHits++; // TLB hit
        } else {
            // TLB miss
            if (tlbMap.size() == K) {
                // TLB is full: find page with farthest next use
                int farthestPage = -1;
                int farthestIndex = -1;

                for (const auto& entry : tlbMap) 
                {
                    int nextIndex = nextUse[entry.second]; //ayush has made changes here
                    if(nextIndex==-1)
                     //ayush has made changes here
                    {
                        farthestPage=entry.first;
                        break;
                    }
                    if (nextIndex > farthestIndex) {
                        farthestIndex = nextIndex;
                        farthestPage = entry.first;
                    }
                }
                // Remove the farthest page from the TLB
                tlbMap.erase(farthestPage);
            }
            // Add new page to the TLB
            tlbMap[pageNumber] = i; // Mark the page as present in TLB
        }
        
        // // Update the next use for the current page
        // for (int j = i + 1; j < N; j++) {
        //     int futurePageNumber = addresses[j] / (P * 1024);
        //     if (nextUse[pageNumber] == -1 && futurePageNumber == pageNumber) {
        //         nextUse[pageNumber] = j; // Update next use index
        //         break;
        //     }
        // }
         //ayush has made changes here
    }

    return tlbHits;
}

// int main() {
//     int T;
//     cin >> T; // Number of test cases

//     while (T--) {
//         int S, P, K, N;
//         cin >> S >> P >> K >> N; // Read input

//         unsigned int addresses[N];
//         for (int i = 0; i < N; i++) {
//             cin >> hex >> addresses[i]; // Read addresses in hexadecimal
//         }

//         // Run the different TLB replacement algorithms
//         int fifoHits = FIFO(S, P, K, N, addresses);
//         int lifoHits = LIFO(S, P, K, N, addresses);
//         int lruHits = LRU(S, P, K, N, addresses);
//         int optHits = Optimal(S, P, K, N, addresses);

//         // Output results
//         cout<<fifoHits<<" "<<lifoHits<<" "<<lruHits<<" "<<optHits<<"\n";
//     }

//     return 0;
// }

// Add debug statements to track execution
//cout << "Processing test case " << T << endl;

int main() {
    int T;
    cin >> T; // Number of test cases
    //cout << "Number of test cases: " << T << endl;

    while (T--) 
    {
        int S, P, K, N;
        cin >> S >> P >> K >> N; // Read input
        //cout << "S: " << S << ", P: " << P << ", K: " << K << ", N: " << N << endl;

        // Ensure that N is valid before allocating addresses array

        unsigned int* addresses = new unsigned int[N]; // Dynamic allocation

        //changes here
        for (int i = 0; i < N; i++) {
            string hexInput;
            cin >> hexInput;
            addresses [i] = stoul(hexInput, nullptr, 16);
            //cout << "Address " << i << ": " << addresses[i] << endl;
        }

        // Run the different TLB replacement algorithms
        int fifoHits = FIFO(S, P, K, N, addresses);
        int lifoHits = LIFO(S, P, K, N, addresses);
        int lruHits = LRU(S, P, K, N, addresses);
        int optHits = Optimal(S, P, K, N, addresses);

        // Output results
        cout <<fifoHits<<" "<<lifoHits<<" "<<lruHits<<" "<<optHits <<endl;

        //delete[] addresses; // Free the allocated memory
    }

    return 0;
}