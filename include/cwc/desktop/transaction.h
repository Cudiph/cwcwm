#ifndef _CWC_TRANSACTION_H
#define _CWC_TRANSACTION_H

#include "cwc/desktop/output.h"

void transaction_schedule_output(struct cwc_output *output);
void transaction_schedule_tag(struct cwc_tag_info *tag);

void transaction_pause();
void transaction_resume();

#endif // !_CWC_TRANSACTION_H
