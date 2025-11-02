#include "statistics.h"
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

Statistics stats;
/**
 * @brief Initializes the global statistics structure.
 *
 * This function sets all fields of the 'stats' structure to zero and
 * records the starting time for the data transfer.
 */
void initStatistics() {
    memset(&stats, 0, sizeof(Statistics));
    
    struct timeval tv;
    gettimeofday(&tv, NULL);
    stats.startTime = tv.tv_sec + tv.tv_usec / 1000000.0;
}
/**
 * @brief Calculates the data transfer throughput.
 *
 * Calculated as (Total Data Bytes * 8 bits/byte) / Elapsed Time in seconds.
 *
 * @return Throughput in bits per second (bps).
 */
double calculateThroughput() {
    if (stats.endTime <= stats.startTime) return 0.0;
    
    double elapsedTime = stats.endTime - stats.startTime;
    return (stats.totalDataBytes * 8.0) / elapsedTime; // bits per second
}
 /**
 * @brief Calculates the Frame Error Rate (FER).
 *
 * FER = (Total Error Frames) / (Total Frames Received)
 *
 * @return Frame Error Rate as a value between 0.0 and 1.0.
 */
double calculateFER() {
    int totalFramesReceived = stats.framesReceivedCorrectly + 
                              stats.bcc1Errors + 
                              stats.bcc2Errors + 
                              stats.duplicateFrames;
    
    if (totalFramesReceived == 0) return 0.0;
    
    int totalErrorFrames = stats.bcc1Errors + stats.bcc2Errors + stats.duplicateFrames;
    return (double)totalErrorFrames / totalFramesReceived;
}

/**
 * @brief Calculates final metrics and prints a formatted report to stdout.
 *
 * Records the end time, calculates throughput, FER, and presents frame, error,
 * and retransmission statistics.
 *
 * @param role The role string ("TRANSMITTER" or "RECEIVER") for the report title.
 */
void printStatistics(const char* role) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    stats.endTime = tv.tv_sec + tv.tv_usec / 1000000.0;
    
    double throughput = calculateThroughput();
    double fer = calculateFER();
    
    printf("\n========================================\n\n");
    printf("        PROTOCOL STATISTICS (%s)\n", role);
    printf("\n========================================\n\n");
    
    printf("DATA TRANSFER:\n");
    printf("  Total data bytes: %lld\n", stats.totalDataBytes);
    printf("  Transfer time: %.3f seconds\n", stats.endTime - stats.startTime);
    printf("  Throughput: %.2f bits/s (%.2f KB/s)\n", 
           throughput, 
           throughput / 8192.0);
    
    printf("\nFRAME STATISTICS:\n");
    printf("  Frames transmitted: %d\n", stats.framesTransmitted);
    printf("  Frames received correctly: %d\n", stats.framesReceivedCorrectly);
    
    int totalFramesReceived = stats.framesReceivedCorrectly + 
        stats.bcc1Errors + 
        stats.bcc2Errors + 
        stats.duplicateFrames;
    if (totalFramesReceived > 0) {
        printf("  Total frames received: %d\n", totalFramesReceived);
        printf("  Success rate: %.2f%%\n", 
               (stats.framesReceivedCorrectly * 100.0) / totalFramesReceived);
    }
    
    if (stats.framesTransmitted > 0) {
        printf("  Frame Error Rate (FER): %.4f (%.2f%%)\n", fer, fer * 100.0);
    }
    printf("\nERRORS & RETRANSMISSIONS:\n");
    printf("  Total error frames: %d\n", 
           stats.bcc1Errors + stats.bcc2Errors + stats.duplicateFrames);
    printf("    - BCC1 errors: %d\n", stats.bcc1Errors);
    printf("    - BCC2 errors: %d\n", stats.bcc2Errors);
    printf("    - Duplicate frames: %d\n", stats.duplicateFrames);
    printf("  Frames retransmitted: %d\n", stats.framesRetransmitted);
    printf("  Timeouts: %d\n", stats.timeouts);
    printf("  REJ sent: %d\n", stats.rejSent);
    printf("  REJ received: %d\n", stats.rejReceived);

    if (stats.framesTransmitted > 0) {
        printf("  Retransmission rate: %.2f%%\n", 
               (stats.framesRetransmitted * 100.0) / stats.framesTransmitted);
    }

    printf("\n========================================\n\n");
}