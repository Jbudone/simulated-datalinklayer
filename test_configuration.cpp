// Link layer parameters
unsigned int timeout = 100000;
unsigned int max_win = 1; // 3
unsigned int num_seq = 1; // 10

// Physical Layer parameters
const int PHYSICAL_LAYER_DELAY = 0;
//const int PHYSICAL_LAYER_DELAY = 1000;

double drop[] = {1.0,0.0,0.0,0.0,0,0};
Impair a_impair(NULL,0, NULL,0, PHYSICAL_LAYER_DELAY);
//Impair a_impair(drop,5, NULL,0, PHYSICAL_LAYER_DELAY);

Impair b_impair(NULL,0, NULL,0, PHYSICAL_LAYER_DELAY);
