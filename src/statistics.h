#ifndef STATISTICS_H
#define STATISTICS_H

typedef struct {
    // Number of frames sent
    int framesTransmitted;
    // Number of frames received correctly
    int framesReceivedCorrectly;
    
    // Retransmissions and timeouts
    int framesRetransmitted;
    int timeouts;
    
    // REJ frames and duplicates
    int rejSent;
    int rejReceived;
    int duplicateFrames;
    
    // Check errors
    int bcc1Errors;
    int bcc2Errors;
    
    // Useful data bytes
    long long totalDataBytes;
    
    // Timing for throughput calculation
    double startTime;
    double endTime;
    
} Statistics;

// Global statistics variable
extern Statistics stats;

// Helper functions
void initStatistics();
void printStatistics(const char* role);
double calculateThroughput();
double calculateFER();

#endif
