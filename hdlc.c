#include <stdio.h>
#include <string.h>
#include <math.h>

#include "hdlc.h"

void hdlc_debug(hdlc_state_t *state) {
    printf("hdlc_state(in_packet=%5s, one_count=%3zu, buff_idx=%3zu)\n",
            state->in_packet ? "true" : "false",
            state->one_count,
            state->buff_idx);
}

void hdlc_init(hdlc_state_t *state) {
    state->in_packet = false;
    state->one_count = 0;
    state->buff_idx = 0;
}

bool hdlc_execute(hdlc_state_t *state, float samp, size_t *len) {
    bool bit = samp >= 0;

    if(bit) {
        state->one_count += 1;

        // Illegal state
        if(state->one_count > 6) {
            state->in_packet = false;
            return false;
        } else if(state->one_count == 6) {
            if(state->in_packet) {
                state->in_packet = false;

                if(state->buff_idx > 6) {
                    *len = state->buff_idx - 6;
                    state->buff_idx = 0;
                    return true;
                }
                state->buff_idx = 0;
                return false;
            }
        }
    } else {
        size_t _one_count = state->one_count;

        // Reset the # of consecutive 1's
        state->one_count = 0;
        
        // Receiving a frame deliminter
        if(_one_count == 6) {
            state->in_packet = true;
            state->buff_idx = 0;
            return false;
        }

        // Receiving a stuffed bit
        else if(_one_count == 5) {
            return false;
        }
    }

    if(state->in_packet) {
        if(state->buff_idx < HDLC_SAMP_BUFF_LEN) {
            state->samps[state->buff_idx] = samp;
            state->buff_idx += 1;
        } else {
            state->buff_idx = 0;
            state->in_packet = false;
            fprintf(stderr, "Uhoh, squelch got left open.\n");
        }
    }

    return false;
}

void dump_packet(uint8_t *buff, size_t len) {
    uint16_t crc = buff[len - 1] << 8 | buff[len - 2];

    size_t i;
    uint8_t byte;

    for(i = 0; i < len; i++) {
        if((i % 16) == 0) {
            printf("%04x - ", (unsigned int)i);
        }
        printf("%02x ", buff[i]);
        if(i > 0 && (i % 16) == 15) {
            printf("\n");
        }
    }
    printf("\n");

    for(i = 0; i < len; i++) {
        byte = buff[i] >> 1;
        if(i > 0 && (i % 7) == 6) {
            printf("-%d\n", byte & 0x0f);
        }
        else if(byte != ' ') {
            if(byte >= 32 && byte < 0x7f) printf("%c", byte);
            else printf(".");
        }

        if(buff[i] & 1) {
            printf("last char at %zu\n", i);
            break;
        }
    }
    printf("\n");

    uint8_t control = buff[i + 1];
    uint8_t pid = buff[i + 2];
    printf("Control: %02x\n", control);
    printf("PID: %02x\n", pid);
    i += 3;
    while(i < (len - 2)) {
        if(buff[i] >= 32 && buff[i] < 0x7f) printf("%c", buff[i]);
        else printf(".");
        i++;
    }
    printf("\n");

    printf("Checksum: %04x\n", crc);
    printf("--------------------------------\n");
}

bool crc16_ccitt(const float *buff, size_t len) {
    uint8_t data[4096];

    if(len < 32) return false;
    if((len % 8) != 0) return false;
    if(len > (4096 * 8)) return false;

    memset(data, 0, sizeof(data));

    uint8_t byte = 0;
    size_t j = 0;
    for(size_t i = 0; i < len; i++) {
        byte >>= 1;
        byte |= (buff[i] >= 0 ? 0x80 : 0);
        if((i % 8) == 7) {
            data[j] = byte;
            j += 1;
        }
    }
    size_t pktlen = j;

    uint16_t ret = calc_crc(data, pktlen - 2);
    uint16_t crc = data[pktlen - 1] << 8 | data[pktlen - 2];

    if(ret == crc || true) {
        printf("Got packet with %zu samps\n", len);
        dump_packet(data, pktlen);
        printf("calc'd crc = 0x%04x\n", ret);
        printf("pkt crc    = 0x%04x\n", crc);
        printf("================================\n");
    }
    return ret == crc;
}

uint16_t calc_crc(uint8_t *data, size_t len) {
    unsigned int POLY=0x8408; //reflected 0x1021
    unsigned short crc=0xFFFF;
    for(size_t i=0; i<len; i++) {
        crc ^= data[i];
        for(size_t j=0; j<8; j++) {
            if(crc&0x01) crc = (crc >> 1) ^ POLY;
            else         crc = (crc >> 1);
        }
    }
    return crc ^ 0xFFFF;
}

void flip_smallest(float *data, size_t len) {
    float min = INFINITY;
    size_t min_idx = 0;

    for(size_t j = 0; j < len; j++) {
        if(fabs(data[j]) < min) {
            min_idx = j;
            min = fabs(data[j]);
        }
    }

    data[min_idx] *= -1;
    data[min_idx] += (data[min_idx] >= 0 ? 1 : -1);
}
