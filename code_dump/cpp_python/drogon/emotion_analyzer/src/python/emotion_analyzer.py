import re
import json
from transformers import pipeline, AutoTokenizer
from datetime import datetime, timedelta


class SubtitleEmotionAnalyzer:

    def __init__(
        self, model_name="j-hartmann/emotion-english-distilroberta-base", max_tokens=512
    ):
        self.classifier = pipeline(
            "text-classification", model=model_name, return_all_scores=True
        )
        self.tokenizer = AutoTokenizer.from_pretrained(model_name)
        self.max_tokens = max_tokens

    def time_to_seconds(self, time_str):
        time_format = "%H:%M:%S,%f"
        return (
            datetime.strptime(time_str, time_format) - datetime(1900, 1, 1)
        ).total_seconds()

    def seconds_to_time(self, seconds):
        return str(timedelta(seconds=seconds))[:-3].replace(".", ",")

    def extract_subtitles_from_content(self, content):
        """
        Extracts subtitles from SRT content string.
        """
        # Normalize newlines to \n
        content = content.replace("\r\n", "\n").replace("\r", "\n")

        # Debug print
        print("Received content:", repr(content))

        # Modified pattern to be more flexible with newlines
        pattern = re.compile(
            r"(\d{2}:\d{2}:\d{2},\d{3})\s*-->\s*(\d{2}:\d{2}:\d{2},\d{3})\s*\n((?:.*?\n)*?)(?:\n|$)",
            re.DOTALL,
        )
        matches = pattern.findall(content)

        print(f"Found {len(matches)} subtitle matches")

        subtitles = []
        for match in matches:
            start_time, end_time, text = match
            text = text.strip().replace("\n", " ")
            if text:
                subtitle = {
                    "start_time": start_time,
                    "end_time": end_time,
                    "text": text,
                }
                subtitles.append(subtitle)
                print(f"Parsed subtitle: {subtitle}")

        return subtitles

    def split_text_into_sentences(self, text):
        sentences = re.split(r"[.!?]+", text)
        return [s.strip() for s in sentences if s.strip()]

    def get_token_length(self, text):
        return len(self.tokenizer.encode(text, add_special_tokens=True))

    def create_subchunks(self, text):
        sentences = self.split_text_into_sentences(text)
        subchunks = []
        current_chunk = ""

        for sentence in sentences:
            potential_chunk = (
                current_chunk + " " + sentence if current_chunk else sentence
            )
            if self.get_token_length(potential_chunk) <= self.max_tokens:
                current_chunk = potential_chunk
            else:
                if current_chunk:
                    subchunks.append(current_chunk)
                current_chunk = sentence

        if current_chunk:
            subchunks.append(current_chunk)

        return subchunks

    def process_text_safely(self, text):

        if not text.strip():
            return None

        try:
            subchunks = self.create_subchunks(text)
            all_emotions = []

            for subchunk in subchunks:
                if self.get_token_length(subchunk) <= self.max_tokens:
                    emotions = self.classifier(subchunk)
                    if emotions and emotions[0]:
                        all_emotions.append(emotions[0])

            if not all_emotions:
                return None

            aggregated_emotions = {}
            for chunk_emotions in all_emotions:
                for emotion in chunk_emotions:
                    label, score = emotion["label"], emotion["score"]
                    if label in aggregated_emotions:
                        aggregated_emotions[label].append(score)
                    else:
                        aggregated_emotions[label] = [score]

            averaged_emotions = {
                label: sum(scores) / len(scores)
                for label, scores in aggregated_emotions.items()
            }

            dominant_emotion = max(averaged_emotions.items(), key=lambda x: x[1])

            return {
                "emotions": averaged_emotions,
                "dominant_emotion": dominant_emotion[0],
                "confidence": dominant_emotion[1],
            }
        except Exception as e:
            print(f"Error processing text: {str(e)}")
            return None

    def create_dynamic_chunks(self, subtitles, target_token_count=200):
        chunks = []
        current_chunk = {
            "start_time": None,
            "end_time": None,
            "text": "",
            "subtitles": [],
        }

        for subtitle in subtitles:
            potential_text = current_chunk["text"] + " " + subtitle["text"]
            if self.get_token_length(potential_text) <= target_token_count:
                if current_chunk["start_time"] is None:
                    current_chunk["start_time"] = subtitle["start_time"]
                current_chunk["end_time"] = subtitle["end_time"]
                current_chunk["text"] = potential_text.strip()
                current_chunk["subtitles"].append(subtitle)
            else:
                if current_chunk["text"]:
                    chunks.append(current_chunk)
                current_chunk = {
                    "start_time": subtitle["start_time"],
                    "end_time": subtitle["end_time"],
                    "text": subtitle["text"],
                    "subtitles": [subtitle],
                }

        if current_chunk["text"]:
            chunks.append(current_chunk)

        return chunks

    def analyze_emotions_in_chunks(self, content):
        """
        Main function to be called from Zig.
        Takes SRT content as string and returns JSON results.
        """

        try:
            # Extract subtitles from content
            subtitles = self.extract_subtitles_from_content(content)

            if not subtitles:
                return json.dumps(
                    {
                        "error": "No subtitles found in content",
                        "timeline": [],
                        "metadata": {"total_chunks": 0, "analyzed_chunks": 0},
                    }
                )

            # Create and analyze chunks
            chunks = self.create_dynamic_chunks(subtitles)
            emotion_results = []

            for chunk in chunks:
                result = self.process_text_safely(chunk["text"])
                if result:
                    emotion_results.append(
                        {
                            "start_time": chunk["start_time"],
                            "end_time": chunk["end_time"],
                            "text": chunk["text"],
                            "emotions": result["emotions"],
                            "dominant_emotion": result["dominant_emotion"],
                            "confidence": result["confidence"],
                        }
                    )

            # Return JSON string
            return json.dumps(
                {
                    "timeline": emotion_results,
                    "metadata": {
                        "total_chunks": len(chunks),
                        "analyzed_chunks": len(emotion_results),
                    },
                }
            )

        except Exception as e:
            return json.dumps(
                {
                    "error": str(e),
                    "timeline": [],
                    "metadata": {"total_chunks": 0, "analyzed_chunks": 0},
                }
            )


def analyze_emotions(content):
    """
    Entry point function to be called from Zig.
    Initializes analyzer and processes content.
    """
    try:
        print("\n=== Starting analysis ===")
        print(f"Content length: {len(content)}")
        print(f"Content type: {type(content)}")
        print("First 200 characters:", repr(content[:200]))

        analyzer = SubtitleEmotionAnalyzer()
        result = analyzer.analyze_emotions_in_chunks(content)

        print("Analysis completed successfully")
        return result

    except Exception as e:
        import traceback

        error_msg = f"Error: {str(e)}\n{traceback.format_exc()}"
        print(error_msg)
        return json.dumps(
            {
                "error": error_msg,
                "timeline": [],
                "metadata": {"total_chunks": 0, "analyzed_chunks": 0},
            }
        )


if __name__ == "__main__":
    # Example usage for testing
    test_content = """1
00:00:01,000 --> 00:00:04,000
Hello world!

2
00:00:05,000 --> 00:00:08,000
This is a test subtitle."""

    result = analyze_emotions(test_content)
    print(result)
