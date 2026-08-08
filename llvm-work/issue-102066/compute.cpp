float compute(const float* v, const unsigned char* m, int size) {
    float vsum = 0.0f;
    int vcount = 0;

    for (int i = 0; i < size; ++i) {
        const float tmp = v[i];
        if (m[i]) {
            vsum += tmp;
            vcount += 1;
        }
    }

    return vsum / vcount;
}
