#include "statistics.h"
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

Statistics stats;

void initStatistics() {
    memset(&stats, 0, sizeof(Statistics));
    
    struct timeval tv;
    gettimeofday(&tv, NULL);
    stats.startTime = tv.tv_sec + tv.tv_usec / 1000000.0;
}

double calculateThroughput() {
    if (stats.endTime <= stats.startTime) return 0.0;
    
    double elapsedTime = stats.endTime - stats.startTime;
    return (stats.totalDataBytes * 8.0) / elapsedTime; // bits per second
}

void printStatistics(const char* role) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    stats.endTime = tv.tv_sec + tv.tv_usec / 1000000.0;
    
    printf("\n========================================\n");
    printf("        PROTOCOL STATISTICS (%s)\n", role);
    printf("========================================\n\n");
    
    printf("DATA TRANSFER:\n");
    printf("  Total data bytes: %lld\n", stats.totalDataBytes);
    printf("  Transfer time: %.2f seconds\n", stats.endTime - stats.startTime);
    printf("  Throughput: %.2f bits/s (%.2f KB/s)\n", 
           calculateThroughput(), 
           calculateThroughput() / 8192.0);
    
    printf("\nFRAME STATISTICS:\n");
    printf("  Frames transmitted: %d\n", stats.framesTransmitted);
    printf("  Frames received correctly: %d\n", stats.framesReceivedCorrectly);
    
    if (stats.framesTransmitted > 0) {
        printf("  Success rate: %.2f%%\n", 
               (stats.framesReceivedCorrectly * 100.0) / stats.framesTransmitted);
    }
    
    printf("\nERRORS & RETRANSMISSIONS:\n");
    printf("  Frames retransmitted: %d\n", stats.framesRetransmitted);
    printf("  Timeouts: %d\n", stats.timeouts);
    printf("  REJ sent: %d\n", stats.rejSent);
    printf("  REJ received: %d\n", stats.rejReceived);
    printf("  Duplicate frames: %d\n", stats.duplicateFrames);
    printf("  BCC1 errors: %d\n", stats.bcc1Errors);
    printf("  BCC2 errors: %d\n", stats.bcc2Errors);
    
    if (stats.framesTransmitted > 0) {
        printf("  Retransmission rate: %.2f%%\n", 
               (stats.framesRetransmitted * 100.0) / stats.framesTransmitted);
    }
    
    printf("\n========================================\n\n");
}