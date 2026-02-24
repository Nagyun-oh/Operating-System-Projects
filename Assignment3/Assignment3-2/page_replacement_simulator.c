#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define MAX_FRAMES 1000 

typedef struct {
    int *refs;  // Array containing the actual Page Numbers
    int n;      // Length of the reference sequence
} RefSeq;

/**
 * read_input - Reads the page frame size and reference string from a file.
 * @path: Path to the input file
 * @frames: Pointer to store the number of frames
 * @seq: Pointer to the RefSeq structure to store page references
 */
static int read_input(const char *path, int *frames, RefSeq *seq) {

    FILE *fp = fopen(path, "r");
     
    // Read the number of page frames
    if (fscanf(fp, "%d", frames) != 1) {
        fclose(fp);
	    fprintf(stderr,"Wrong input 1\n");
        return -1;
    }

    // Validate frame count range
    if (*frames <= 0 || *frames > MAX_FRAMES) {
        fclose(fp);
	    fprintf(stderr,"Wrong input 2\n");
        return -1;
    }

    // Allocate memory for the reference sequence (initial capacity: 128)
    int cap = 128;
    seq->refs = (int*)malloc(sizeof(int)*cap);
    seq->n = 0;

    // Read page numbers until the end of the file
    int x;
    while (fscanf(fp, "%d", &x) == 1) {
        // Resize the array if the capacity is exceeded
        if (seq->n == cap) {
            cap *= 2;
            seq->refs = (int*)realloc(seq->refs, sizeof(int)*cap);
        }
        seq->refs[seq->n++] = x; // Store page number in the sequence
    }

    fclose(fp);
    return 0;
}

/**
 * find_in_frames - Checks if a specific page exists in the current frames.
 * @frames: Current page frames array
 * @f: Total number of frames
 * @page: Target page number to search for
 * Returns the index if found, otherwise returns -1.
 */
static int find_in_frames(const int *frames, int f, int page) {

    for (int i = 0; i < f; i++) 
        if (frames[i] == page) return i; // Returns if the index containing the page is found

    return -1; 
}

/**
 * simulate_opt - Optimal Page Replacement Simulation
 * Logic:
 * - HIT: Page exists in frames, proceed to next reference.
 * - MISS:
 * (1) If there is an empty frame, use it.
 * (2) Otherwise, replace the page that will not be used for the longest period in the future.
 */
static int simulate_opt(int F, const RefSeq *seq) {
    
    int *frames = (int*)malloc(sizeof(int)*F);
    for (int i = 0; i < F; ++i) frames[i] = -1; // Initialize as empty (-1)

    int faults = 0; // Page faluts
    int filled = 0; // Number of currently occupied frames

    for (int k = 0; k < seq->n; k++) {

        int p = seq->refs[k]; // currently referring page

        int hit_idx = find_in_frames(frames, F, p);
        if (hit_idx != -1) continue; // if hit, continue

        faults++; // MISS occured

        // 1) Fill empty frame if available
        if (filled < F) {                      
            frames[filled++] = p;
            continue;
        }

        // 2) Select a victim for replacement
        //   - Priority 1: A page that is never referenced again.
        //   - Priority 2: A page whose next reference is the furthest in the future.
        int found = 0;
	    int furthest = -1; 
        for (int i = 0; i < F; i++) {
            int next = -1;
		
            // Search for the next occurrence of frames[i]
            for (int j = k+1; j < seq->n; j++) {
                if (seq->refs[j] == frames[i]) {
		       	    next = j; 
			        break;
                }
            }
            // Replace immediately if the page will never be used again
            if (next == -1) {
		        found = i;
		        break;
            }   
            // Track the page with the furthest next reference
            if (next > furthest) {
                furthest = next; 
                found = i; 
            }
        }

        frames[found] = p; // Perform replacement
    }

    free(frames);
    return faults;
}

/**
 * simulate_fifo - First-In, First-Out (FIFO) Page Replacement
 * Logic:
 * - Replaces the oldest page in the frames.
 * - idx: Points to the frame that was loaded first (circular queue behavior).
 */
static int simulate_fifo(int F, const RefSeq *seq) {

    int *frames = (int*)malloc(sizeof(int)*F);
    for (int i = 0; i < F; i++) frames[i] = -1; // Initialize as empty (-1)

    int faults = 0; 
    int idx = 0; // Pointer to the next victim frame
    int  filled = 0; 

    for (int t = 0; t < seq->n; t++) {
        int p = seq->refs[t];  

        if (find_in_frames(frames, F, p) != -1) continue; // HIT
        faults++; // MISS 

	    // If the frame is still empty, fill it in.
        if (filled < F) {
            frames[filled++] = p;
        } else {
            frames[idx] = p; // Replace the oldest frame when it is full
            idx = (idx + 1) % F; 
        }
    }

    free(frames);
    return faults;
}

/**
 * simulate_LRU - Least Recently Used (LRU) Page Replacement
 * Logic:
 * - frames[i]: Page number currently held in frame i.
 * - last[i]: Timestamp of the last usage of frame i.
 * - Victim selection: Replace the frame with the smallest last[i] value.
 */
static int simulate_lru(int F, const RefSeq *seq) {

    int *frames = (int*)malloc(sizeof(int)*F);
    int *last   = (int*)malloc(sizeof(int)*F);

    // Initialize as empty (-1)
    for (int i = 0; i < F; i++) {
	    frames[i] = -1;
	    last[i] = -1; 
    }

    int faults = 0;
    int filled = 0;

    for (int t = 0; t < seq->n; t++) {

        int p = seq->refs[t]; 
        int idx = find_in_frames(frames, F, p); 

	    // HIT : Update last-used timestamp
        if (idx != -1) {   
            last[idx] = t;
            continue;
        }
	    // MISS
        faults++;

	    // Fill empty frame
        if (filled < F) {
            frames[filled] = p;
            last[filled] = t;
	        filled++;    
        }else {
            // Find the LRU victim (oldest timestamp)
            int victim = 0;
            for (int i = 1; i < F; i++)
                if (last[i] < last[victim]) victim = i; 
            frames[victim] = p;
            last[victim]   = t;
        }
    }

    free(frames);
    free(last);
    return faults;
}

/**
 * simulate_clock - Clock (Second-Chance) Algorithm
 * Logic:
 * - refb[i]: Reference bit (0 or 1). Set to 1 when a page is accessed.
 * - hand: A pointer (clock hand) that traverses frames to find a victim.
 * - If hand finds refb == 1, it clears the bit (0) and moves to the next frame (second chance).
 * - If hand finds refb == 0, that frame is selected for replacement.
 */
static int simulate_clock(int F, const RefSeq *seq) {

    // Initialzie
    int *frames = (int*)malloc(sizeof(int)*F);
    char *refb  = (char*)malloc(sizeof(char)*F);
    for (int i = 0; i < F; i++) {
	    frames[i] = -1; 
	    refb[i] = 0; 
    }

    int faults = 0; 
    int hand = 0; // Clock hand index
    int filled = 0; 

    for (int t = 0; t < seq->n; t++) {

        int p = seq->refs[t]; 

	    // Check for HIT
        int idx = find_in_frames(frames, F, p); 
        if (idx != -1) {
	       	refb[idx] = 1;
	       	continue;
       	} 

	    // MISS
        faults++;

        // 1) Fill empty frame if available
        if (filled < F) {
            frames[filled] = p;
            refb[filled] = 1;
	        filled++;
        }
        // 2) Perform Clock replacement if frames are full
        else {
            while(1) {
                // If reference bit is 0, replace the page
                if (refb[hand] == 0) {
                    frames[hand] = p;
                    refb[hand] = 1; // New page gets its bit set to 1
                    hand = (hand + 1) % F; // Advance hand
                    break;
                } else {
                    // Give a second chance: clear bit and advance hand
                    refb[hand] = 0;
                    hand = (hand + 1) % F;
                }
            }
        }
    }

    free(frames);
    free(refb);
    return faults;
}

/**
 * print_result - Calculates and displays the simulation results.
 */
static void print_result(const char *title, int faults, int total_refs) {

    // Calculate Page Falut rate 
    double rate = 0;
    
    if (total_refs != 0) {
	    rate = (faults * 100.0) / (double)total_refs;
    }
    printf("%s\n", title); 
    printf("Number of Page Faults: %d\n", faults); 
    printf("Page Fault Rate: %.2f%%\n\n", rate); 
}

int main(int argc, char **argv) {

    if (argc != 2) {
        fprintf(stderr, "Wrong input\n");
        return 1;
    }

    int frames; 
    RefSeq sequence = {0}; 
	
    read_input(argv[1],&frames,&sequence);

    // Run simulations for each of the four algorithms
    int opt_faults   = simulate_opt(frames, &sequence);
    int fifo_faults  = simulate_fifo(frames, &sequence);
    int lru_faults   = simulate_lru(frames, &sequence);
    int clock_faults = simulate_clock(frames, &sequence);

    // Output results
    print_result("Optimal Algorithm:", opt_faults, sequence.n);
    print_result("FIFO Algorithm:",    fifo_faults, sequence.n);
    print_result("LRU Algorithm:",     lru_faults, sequence.n);
    print_result("Clock Algorithm:",   clock_faults, sequence.n);

    // Cleanup resources
    free(sequence.refs);
    return 0;
}

