#include "status_message.h"

void status_init(status_message *s)
{
	s->text = 0;
	s->length = 0;
	s->frames = 0;
}

void status_show(status_message *s, const char *text)
{
	int length = 0;

	while (text[length] != 0) // once per message, not per frame
		length++;
	s->text = text;
	s->length = (unsigned char)length;
	s->frames = 0;
}

// The frame on which typing is done: every character's 2 frames (a shift, not a multiply)
static int typed_by(const status_message *s)
{
	return s->length << STATUS_TYPE_SHIFT;
}

void status_update(status_message *s)
{
	if (s->text == 0)
		return;
	s->frames++;
	if (s->frames >= typed_by(s) + STATUS_HOLD_FRAMES)
		status_init(s); // up long enough: gone
}

int status_visible(const status_message *s)
{
	return s->text != 0;
}

int status_typed_chars(const status_message *s)
{
	int typed = s->frames >> STATUS_TYPE_SHIFT;

	return typed < s->length ? typed : s->length;
}

int status_cursor_on(const status_message *s)
{
	const int done = typed_by(s);

	if (s->text == 0)
		return 0;
	if (s->frames < done)
		return 1; // solid while it types
	return (((s->frames - done) >> STATUS_BLINK_SHIFT) & 1) == 0; // then on, off, on...
}
