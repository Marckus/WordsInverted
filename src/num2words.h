#pragma once

#include "pebble.h"
#define BUFFER_SIZE 43
#define DATE_FORMAT "%a, %b %e"

void fuzzy_minutes_to_words(struct tm *t, char* words);
void fuzzy_hours_to_words(struct tm *t, char* words);
void fuzzy_sminutes_to_words(struct tm *t, char* words);
void fuzzy_dates_to_words(struct tm *t, char* words);