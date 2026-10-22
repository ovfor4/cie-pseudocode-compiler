struct Sample {
    char tag;
    double value;
    int count;
};

static struct Sample shared = {'G', 0.5, 7};

void scale_sample(struct Sample *s, int k) { s->value *= k; }
struct Sample *shared_sample(void) { return &shared; }
int shared_count(void) { return shared.count; }
