/*
 * Copyright 2013-2014, Stephan Aßmus <superstippi@gmx.de>.
 * All rights reserved. Distributed under the terms of the MIT License.
 */

#include "TextDocument.h"

#include <algorithm>
#include <stdio.h>


TextDocument::TextDocument()
	:
	fParagraphs(),
	fEmptyLastParagraph(),
	fDefaultCharacterStyle()
{
}


TextDocument::TextDocument(const CharacterStyle& characterStyle,
	const ParagraphStyle& paragraphStyle)
	:
	fParagraphs(),
	fEmptyLastParagraph(paragraphStyle),
	fDefaultCharacterStyle(characterStyle)
{
}


TextDocument::TextDocument(const TextDocument& other)
	:
	fParagraphs(other.fParagraphs),
	fEmptyLastParagraph(other.fEmptyLastParagraph),
	fDefaultCharacterStyle(other.fDefaultCharacterStyle)
{
}


TextDocument&
TextDocument::operator=(const TextDocument& other)
{
	fParagraphs = other.fParagraphs;
	fEmptyLastParagraph = other.fEmptyLastParagraph;
	fDefaultCharacterStyle = other.fDefaultCharacterStyle;

	return *this;
}


bool
TextDocument::operator==(const TextDocument& other) const
{
	if (this == &other)
		return true;

	return fEmptyLastParagraph == other.fEmptyLastParagraph
		&& fDefaultCharacterStyle == other.fDefaultCharacterStyle
		&& fParagraphs == other.fParagraphs;
}


bool
TextDocument::operator!=(const TextDocument& other) const
{
	return !(*this == other);
}


// #pragma mark -


status_t
TextDocument::Insert(int32 textOffset, const BString& text)
{
	return Insert(textOffset, text, CharacterStyleAt(textOffset));
}


status_t
TextDocument::Insert(int32 textOffset, const BString& text,
	const CharacterStyle& style)
{
	return Insert(textOffset, text, style, ParagraphStyleAt(textOffset));
}


status_t
TextDocument::Insert(int32 textOffset, const BString& text,
	const CharacterStyle& characterStyle, const ParagraphStyle& paragraphStyle)
{
	int32 paragraphOffset;
	int32 index = ParagraphIndexFor(textOffset, paragraphOffset);
	if (index < 0)
		return B_BAD_VALUE;

	textOffset -= paragraphOffset;

	bool hasLineBreaks = text.FindFirst('\n', 0) >= 0;

	if (hasLineBreaks) {
		// Split paragraph at textOffset
		Paragraph paragraph1(ParagraphAt(index).Style());
		Paragraph paragraph2(paragraphStyle);
		const TextSpanList& textSpans = ParagraphAt(index).TextSpans();
		int32 spanCount = textSpans.CountItems();
		for (int32 i = 0; i < spanCount; i++) {
			const TextSpan& span = textSpans.ItemAtFast(i);
			int32 spanLength = span.CountChars();
			if (textOffset >= spanLength) {
				paragraph1.Append(span);
				textOffset -= spanLength;
			} else if (textOffset > 0) {
				paragraph1.Append(
					span.SubSpan(0, textOffset));
				paragraph2.Append(
					span.SubSpan(textOffset, spanLength - textOffset));
				textOffset = 0;
			} else {
				paragraph2.Append(span);
			}
		}

		fParagraphs.Remove(index);

		// Insert TextSpans, splitting 'text' into Paragraphs at line breaks.
		int32 length = text.CountChars();
		int32 chunkStart = 0;
		while (chunkStart < length) {
			int32 chunkEnd = text.FindFirst('\n', chunkStart);
			bool foundLineBreak = chunkEnd >= chunkStart;
			if (foundLineBreak)
				chunkEnd++;
			else
				chunkEnd = length;

			BString chunk;
			text.CopyCharsInto(chunk, chunkStart, chunkEnd - chunkStart);
			TextSpan span(chunk, characterStyle);

			if (foundLineBreak) {
				if (!paragraph1.Append(span))
					return B_NO_MEMORY;
				if (paragraph1.Length() > 0) {
					if (!fParagraphs.Add(paragraph1, index))
						return B_NO_MEMORY;
					index++;
				}
				paragraph1 = Paragraph(paragraphStyle);
			} else {
				if (!paragraph2.Prepend(span))
					return B_NO_MEMORY;
			}

			chunkStart = chunkEnd + 1;
		}

		if (paragraph2.IsEmpty()) {
			// Make sure Paragraph has at least one TextSpan, even
			// if its empty.
			const TextSpanList& spans = paragraph1.TextSpans();
			const TextSpan& span = spans.LastItem();
			paragraph2.Append(TextSpan("", span.Style()));
		}

		if (!fParagraphs.Add(paragraph2, index))
			return B_NO_MEMORY;
	} else {
		Paragraph paragraph(ParagraphAt(index));
		paragraph.Insert(textOffset, TextSpan(text, characterStyle));
		if (!fParagraphs.Replace(index, paragraph))
			return B_NO_MEMORY;
	}

	return B_OK;
}


// #pragma mark -


status_t
TextDocument::Remove(int32 textOffset, int32 length)
{
	if (length == 0)
		return B_OK;

	int32 paragraphOffset;
	int32 index = ParagraphIndexFor(textOffset, paragraphOffset);
	if (index < 0)
		return B_BAD_VALUE;

	textOffset -= paragraphOffset;

	// The paragraph at the text offset remains, even if the offset is at
	// the beginning of that paragraph. The idea is that the selection start
	// stays visually in the same place. Therefore, the paragraph at that
	// offset has to keep the paragraph style from that paragraph.

	Paragraph resultParagraph(ParagraphAt(index));
	int32 paragraphLength = resultParagraph.Length();
	if (textOffset == 0 && length > paragraphLength) {
		length -= paragraphLength;
		paragraphLength = 0;
		resultParagraph.Clear();
	} else {
		int32 removeLength = std::min(length, paragraphLength - textOffset);
		resultParagraph.Remove(textOffset, removeLength);
		paragraphLength -= removeLength;
		length -= removeLength;
	}

	if (textOffset == paragraphLength && length == 0
		&& index + 1 < fParagraphs.CountItems()) {
		// Line break between paragraphs got removed. Shift the next
		// paragraph's text spans into the resulting one.

		const TextSpanList&	textSpans = ParagraphAt(index + 1).TextSpans();
		int32 spanCount = textSpans.CountItems();
		for (int32 i = 0; i < spanCount; i++) {
			const TextSpan& span = textSpans.ItemAtFast(i);
			resultParagraph.Append(span);
		}
		fParagraphs.Remove(index + 1);
	}

	textOffset = 0;

	while (length > 0 && index + 1 < fParagraphs.CountItems()) {
		const Paragraph& paragraph = ParagraphAt(index + 1);
		paragraphLength = paragraph.Length();
		// Remove paragraph in any case. If some of it remains, the last
		// paragraph to remove is reached, and the remaining spans are
		// transfered to the result parahraph.
		if (length >= paragraphLength) {
			length -= paragraphLength;
			fParagraphs.Remove(index);
		} else {
			// Last paragraph reached
			int32 removedLength = std::min(length, paragraphLength);
			Paragraph newParagraph(paragraph);
			fParagraphs.Remove(index + 1);

			if (!newParagraph.Remove(0, removedLength))
				return B_NO_MEMORY;

			// Transfer remaining spans to resultParagraph
			const TextSpanList&	textSpans = newParagraph.TextSpans();
			int32 spanCount = textSpans.CountItems();
			for (int32 i = 0; i < spanCount; i++) {
				const TextSpan& span = textSpans.ItemAtFast(i);
				resultParagraph.Append(span);
			}

			break;
		}
	}

	fParagraphs.Replace(index, resultParagraph);

	return B_OK;
}


// #pragma mark -


status_t
TextDocument::Replace(int32 textOffset, int32 length, const BString& text)
{
	return Replace(textOffset, length, text, CharacterStyleAt(textOffset));
}


status_t
TextDocument::Replace(int32 textOffset, int32 length, const BString& text,
	const CharacterStyle& style)
{
	return Replace(textOffset, length, text, style,
		ParagraphStyleAt(textOffset));
}


status_t
TextDocument::Replace(int32 textOffset, int32 length, const BString& text,
	const CharacterStyle& characterStyle, const ParagraphStyle& paragraphStyle)
{
	status_t ret = Remove(textOffset, length);
	if (ret != B_OK)
		return ret;

	return Insert(textOffset, text, characterStyle, paragraphStyle);
}


// #pragma mark -


const CharacterStyle&
TextDocument::CharacterStyleAt(int32 textOffset) const
{
	int32 paragraphOffset;
	const Paragraph& paragraph = ParagraphAt(textOffset, paragraphOffset);

	textOffset -= paragraphOffset;
	const TextSpanList& spans = paragraph.TextSpans();

	int32 index = 0;
	while (index < spans.CountItems()) {
		const TextSpan& span = spans.ItemAtFast(index);
		if (textOffset - span.CountChars() < 0)
			return span.Style();
		textOffset -= span.CountChars();
		index++;
	}

	return fDefaultCharacterStyle;
}


const ParagraphStyle&
TextDocument::ParagraphStyleAt(int32 textOffset) const
{
	int32 paragraphOffset;
	return ParagraphAt(textOffset, paragraphOffset).Style();
}


// #pragma mark -


int32
TextDocument::ParagraphIndexFor(int32 textOffset, int32& paragraphOffset) const
{
	// TODO: Could binary search the Paragraphs if they were wrapped in classes
	// that knew there text offset in the document.
	int32 textLength = 0;
	paragraphOffset = 0;
	int32 count = fParagraphs.CountItems();
	for (int32 i = 0; i < count; i++) {
		const Paragraph& paragraph = fParagraphs.ItemAtFast(i);
		int32 paragraphLength = paragraph.Length();
		textLength += paragraphLength;
		if (textLength > textOffset
			|| (i == count - 1 && textLength == textOffset)) {
			return i;
		}
		paragraphOffset += paragraphLength;
	}
	return -1;
}


const Paragraph&
TextDocument::ParagraphAt(int32 textOffset, int32& paragraphOffset) const
{
	int32 index = ParagraphIndexFor(textOffset, paragraphOffset);
	if (index >= 0)
		return fParagraphs.ItemAtFast(index);

	return fEmptyLastParagraph;
}


const Paragraph&
TextDocument::ParagraphAt(int32 index) const
{
	if (index >= 0 && index < fParagraphs.CountItems())
		return fParagraphs.ItemAtFast(index);
	return fEmptyLastParagraph;
}


bool
TextDocument::Append(const Paragraph& paragraph)
{
	return fParagraphs.Add(paragraph);
}


int32
TextDocument::Length() const
{
	// TODO: Could be O(1) if the Paragraphs were wrapped in classes that
	// knew there text offset in the document.
	int32 textLength = 0;
	int32 count = fParagraphs.CountItems();
	for (int32 i = 0; i < count; i++) {
		const Paragraph& paragraph = fParagraphs.ItemAtFast(i);
		textLength += paragraph.Length();
	}
	return textLength;
}


BString
TextDocument::Text() const
{
	return Text(0, Length());
}


BString
TextDocument::Text(int32 start, int32 length) const
{
	if (start < 0)
		start = 0;

	BString text;

	int32 count = fParagraphs.CountItems();
	for (int32 i = 0; i < count; i++) {
		const Paragraph& paragraph = fParagraphs.ItemAtFast(i);
		int32 paragraphLength = paragraph.Length();
		if (paragraphLength == 0)
			continue;
		if (start > paragraphLength) {
			// Skip paragraph if its before start
			start -= paragraphLength;
			continue;
		}

		// Remaining paragraph length after start
		paragraphLength -= start;
		int32 copyLength = std::min(paragraphLength, length);

		text << paragraph.Text(start, copyLength);

		length -= copyLength;
		if (length == 0)
			break;

		// Next paragraph is copied from its beginning
		start = 0;
	}

	return text;
}


TextDocumentRef
TextDocument::SubDocument(int32 start, int32 length) const
{
	TextDocumentRef result(new(std::nothrow) TextDocument(
		fDefaultCharacterStyle, fEmptyLastParagraph.Style()), true);

	if (result.Get() == NULL)
		return result;

	if (start < 0)
		start = 0;

	int32 count = fParagraphs.CountItems();
	for (int32 i = 0; i < count; i++) {
		const Paragraph& paragraph = fParagraphs.ItemAtFast(i);
		int32 paragraphLength = paragraph.Length();
		if (paragraphLength == 0)
			continue;
		if (start > paragraphLength) {
			// Skip paragraph if its before start
			start -= paragraphLength;
			continue;
		}

		// Remaining paragraph length after start
		paragraphLength -= start;
		int32 copyLength = std::min(paragraphLength, length);

		result->Append(paragraph.SubParagraph(start, copyLength));

		length -= copyLength;
		if (length == 0)
			break;

		// Next paragraph is copied from its beginning
		start = 0;
	}

	return result;
}


// #pragma mark -


void
TextDocument::PrintToStream() const
{
	int32 paragraphCount = fParagraphs.CountItems();
	if (paragraphCount == 0) {
		printf("<document/>\n");
		return;
	}
	printf("<document>\n");
	for (int32 i = 0; i < paragraphCount; i++) {
		fParagraphs.ItemAtFast(i).PrintToStream();
	}
	printf("</document>\n");
}


// #pragma mark -


bool
TextDocument::AddListener(const TextListenerRef& listener)
{
	return fTextListeners.Add(listener);
}


bool
TextDocument::RemoveListener(const TextListenerRef& listener)
{
	return fTextListeners.Remove(listener);
}


bool
TextDocument::AddUndoListener(const UndoableEditListenerRef& listener)
{
	return fUndoListeners.Add(listener);
}


bool
TextDocument::RemoveUndoListener(const UndoableEditListenerRef& listener)
{
	return fUndoListeners.Remove(listener);
}


void
TextDocument::_NotifyTextChanging(TextChangingEvent& event) const
{
	// Copy listener list to have a stable list in case listeners
	// are added/removed from within the notification hook.
	TextListenerList listeners(fTextListeners);
	int32 count = listeners.CountItems();
	for (int32 i = 0; i < count; i++) {
		const TextListenerRef& listener = listeners.ItemAtFast(i);
		if (listener.Get() == NULL)
			continue;
		listener->TextChanging(event);
		if (event.IsCanceled())
			break;
	}
}


void
TextDocument::_NotifyTextChanged(const TextChangedEvent& event) const
{
	// Copy listener list to have a stable list in case listeners
	// are added/removed from within the notification hook.
	TextListenerList listeners(fTextListeners);
	int32 count = listeners.CountItems();
	for (int32 i = 0; i < count; i++) {
		const TextListenerRef& listener = listeners.ItemAtFast(i);
		if (listener.Get() == NULL)
			continue;
		listener->TextChanged(event);
	}
}


void
TextDocument::_NotifyUndoableEditHappened(const UndoableEditRef& edit) const
{
	// Copy listener list to have a stable list in case listeners
	// are added/removed from within the notification hook.
	UndoListenerList listeners(fUndoListeners);
	int32 count = listeners.CountItems();
	for (int32 i = 0; i < count; i++) {
		const UndoableEditListenerRef& listener = listeners.ItemAtFast(i);
		if (listener.Get() == NULL)
			continue;
		listener->UndoableEditHappened(this, edit);
	}
}
