#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <omp.h>

typedef struct {
    char id[5]; int llegada; float peso, vol, v_orig, v_efec, uso_rel, ratio;
} Paquete;

typedef struct { char id[5]; float vol_max, peso_max; } Container;

float max_val = 0, best_peso = 0, best_vol = 0;
bool best_comb[20];

float calc_efectivo(float v, float vol_p, float vol_c) {
    float uso = vol_p / vol_c;
    return (uso < 0.10) ? v : ((uso <= 0.20) ? v * 0.95 : v * 0.90);
}

float calc_uso_rel(float p, float cp, float v, float cv) {
    return (p / cp) + (v / cv);
}

int cmp_ratio(const void *a, const void *b) {
    float rA = ((Paquete*)a)->ratio, rB = ((Paquete*)b)->ratio;
    return (rA < rB) - (rA > rB);
}

float calc_cota(int idx, float p_act, float v_act, float val_act, Paquete *paqs, int n, Container c) {
    float bound = val_act, p_disp = c.peso_max - p_act, v_disp = c.vol_max - v_act;
    for (int i = idx; i < n; i++) {
        if (paqs[i].peso <= p_disp && paqs[i].vol <= v_disp) {
            bound += paqs[i].v_efec; p_disp -= paqs[i].peso; v_disp -= paqs[i].vol;
        } else {
            float frac = (p_disp / paqs[i].peso < v_disp / paqs[i].vol) ? p_disp / paqs[i].peso : v_disp / paqs[i].vol;
            if (frac > 0) bound += paqs[i].v_efec * frac;
            break;
        }
    }
    return bound;
}

void b_and_b(int idx, float p_act, float v_act, float val_act, bool *sel, Paquete *paqs, int n, Container c) {
    if (idx == n) {
        #pragma omp critical
        {
            if (val_act > max_val) {
                max_val = val_act; best_peso = p_act; best_vol = v_act;
                memcpy(best_comb, sel, sizeof(bool) * n);
            }
        }
        return;
    }
    if (calc_cota(idx, p_act, v_act, val_act, paqs, n, c) <= max_val) return;

    if (p_act + paqs[idx].peso <= c.peso_max && v_act + paqs[idx].vol <= c.vol_max) {
        sel[idx] = true;
        b_and_b(idx + 1, p_act + paqs[idx].peso, v_act + paqs[idx].vol, val_act + paqs[idx].v_efec, sel, paqs, n, c);
    }
    sel[idx] = false;
    b_and_b(idx + 1, p_act, v_act, val_act, sel, paqs, n, c);
}

int main() {
    Paquete t_paqs[] = {
        {"P1",480,120,8,80,0,0,0}, {"P2",490,300,20,120,0,0,0}, {"P3",500,250,12,150,0,0,0},
        {"P4",510,100,5,60,0,0,0}, {"P5",520,180,15,100,0,0,0}, {"P6",540,400,25,170,0,0,0},
        {"P7",555,90,3,55,0,0,0}, {"P8",570,220,10,130,0,0,0}, {"P9",585,150,7,90,0,0,0}, {"P10",600,350,18,160,0,0,0}
    };
    Container conts[] = {{"C1",30,600}, {"C2",45,850}, {"C3",60,1000}};
    int v_min[] = {480, 540, 600}, v_max[] = {539, 599, 659};
    char *v_nom[] = {"08:00-08:59", "09:00-09:59", "10:00-10:59"};

    for (int v = 0; v < 3; v++) {
        printf("\nVentana: %s\n-----------------------\n", v_nom[v]);
        for (int c = 0; c < 3; c++) {
            Paquete p_v[10]; int n = 0;
            
            for (int i = 0; i < 10; i++) {
                if (t_paqs[i].llegada >= v_min[v] && t_paqs[i].llegada <= v_max[v]) {
                    p_v[n] = t_paqs[i];
                    p_v[n].v_efec = calc_efectivo(p_v[n].v_orig, p_v[n].vol, conts[c].vol_max);
                    p_v[n].uso_rel = calc_uso_rel(p_v[n].peso, conts[c].peso_max, p_v[n].vol, conts[c].vol_max);
                    p_v[n].ratio = p_v[n].v_efec / p_v[n].uso_rel;
                    n++;
                }
            }
            if (n == 0) continue;
            
            qsort(p_v, n, sizeof(Paquete), cmp_ratio);
            max_val = best_peso = best_vol = 0;

            printf("Container: %.0f m3\n", conts[c].vol_max);

            #pragma omp parallel sections
            {
                #pragma omp section
                {
                    printf("  Thread %d explora subarbol 0\n", omp_get_thread_num());
                    bool sel[20] = {false};
                    if (p_v[0].peso <= conts[c].peso_max && p_v[0].vol <= conts[c].vol_max) {
                        sel[0] = true; b_and_b(1, p_v[0].peso, p_v[0].vol, p_v[0].v_efec, sel, p_v, n, conts[c]);
                    }
                }
                #pragma omp section
                {
                    printf("  Thread %d explora subarbol 1\n", omp_get_thread_num());
                    bool sel[20] = {false};
                    b_and_b(1, 0, 0, 0, sel, p_v, n, conts[c]);
                }
            }

            printf("  Mejor valor: %.2f | Peso: %.0f/%.0f | Vol: %.0f/%.0f\n  Paquetes: ", max_val, best_peso, conts[c].peso_max, best_vol, conts[c].vol_max);
            for (int i = 0; i < n; i++) if (best_comb[i]) printf("%s ", p_v[i].id);
            printf("\n\n");
        }
    }
    return 0;
}
