using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

namespace Rtsl.LanguageServer
{
    internal static class Program
    {
        private static readonly Dictionary<string, string> Documents = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);

        private static void Main()
        {
            var stdin = Console.OpenStandardInput();
            var stdout = Console.OpenStandardOutput();

            while (true)
            {
                string message = ReadMessage(stdin);
                if (message == null)
                    break;

                if (message.IndexOf("\"method\":\"initialize\"", StringComparison.Ordinal) >= 0)
                {
                    WriteJson(stdout, ExtractId(message), "{\"capabilities\":{\"textDocumentSync\":1,\"semanticTokensProvider\":{\"legend\":{\"tokenTypes\":[\"keyword\",\"string\",\"number\",\"comment\",\"operator\",\"variable\",\"type\"],\"tokenModifiers\":[]},\"full\":true}}}");
                }
                else if (message.IndexOf("\"method\":\"textDocument/didOpen\"", StringComparison.Ordinal) >= 0)
                {
                    var uri = ExtractString(message, "\"uri\":\"");
                    var text = ExtractDidOpenText(message);
                    if (uri != null)
                    {
                        Documents[uri] = text ?? string.Empty;
                        PublishDiagnostics(stdout, uri, Documents[uri]);
                    }
                }
                else if (message.IndexOf("\"method\":\"textDocument/didChange\"", StringComparison.Ordinal) >= 0)
                {
                    var uri = ExtractString(message, "\"uri\":\"");
                    var text = ExtractDidChangeText(message);
                    if (uri != null)
                    {
                        Documents[uri] = text ?? string.Empty;
                        PublishDiagnostics(stdout, uri, Documents[uri]);
                    }
                }
                else if (message.IndexOf("\"method\":\"textDocument/semanticTokens/full\"", StringComparison.Ordinal) >= 0)
                {
                    var uri = ExtractString(message, "\"uri\":\"");
                    var text = uri != null && Documents.TryGetValue(uri, out var docText) ? docText : string.Empty;
                    WriteJson(stdout, ExtractId(message), "{\"data\":" + BuildSemanticTokens(text) + "}");
                }
                else if (message.IndexOf("\"method\":\"shutdown\"", StringComparison.Ordinal) >= 0)
                {
                    WriteJson(stdout, ExtractId(message), "null");
                }
                else if (message.IndexOf("\"method\":\"exit\"", StringComparison.Ordinal) >= 0)
                {
                    break;
                }
            }
        }

        private static string ReadMessage(Stream input)
        {
            var header = new List<byte>();
            while (true)
            {
                int b = input.ReadByte();
                if (b < 0)
                    return null;
                header.Add((byte)b);
                int count = header.Count;
                if (count >= 4 && header[count - 4] == '\r' && header[count - 3] == '\n' && header[count - 2] == '\r' && header[count - 1] == '\n')
                    break;
            }

            int contentLength = 0;
            string headerText = Encoding.ASCII.GetString(header.ToArray());
            foreach (var line in headerText.Split(new[] { "\r\n" }, StringSplitOptions.RemoveEmptyEntries))
            {
                if (line.StartsWith("Content-Length:", StringComparison.OrdinalIgnoreCase))
                {
                    contentLength = int.Parse(line.Substring("Content-Length:".Length).Trim());
                    break;
                }
            }

            if (contentLength <= 0)
                return null;

            var payload = new byte[contentLength];
            int read = 0;
            while (read < contentLength)
            {
                int n = input.Read(payload, read, contentLength - read);
                if (n <= 0)
                    return null;
                read += n;
            }

            return Encoding.UTF8.GetString(payload);
        }

        private static void WriteJson(Stream output, string id, string resultJson)
        {
            WriteJson(output, "{\"jsonrpc\":\"2.0\",\"id\":" + id + ",\"result\":" + resultJson + "}");
        }

        private static void WriteJson(Stream output, string json)
        {
            var payload = Encoding.UTF8.GetBytes(json);
            var header = Encoding.ASCII.GetBytes("Content-Length: " + payload.Length + "\r\n\r\n");
            output.Write(header, 0, header.Length);
            output.Write(payload, 0, payload.Length);
            output.Flush();
        }

        private static string ExtractString(string text, string marker)
        {
            int start = text.IndexOf(marker, StringComparison.Ordinal);
            if (start < 0)
                return null;
            start += marker.Length;
            int end = text.IndexOf('"', start);
            if (end < 0)
                return null;
            return text.Substring(start, end - start);
        }

        private static string ExtractDidOpenText(string message)
        {
            int marker = message.IndexOf("\"text\":\"", StringComparison.Ordinal);
            if (marker < 0)
                return string.Empty;
            marker += "\"text\":\"".Length;
            int end = message.LastIndexOf("\"", StringComparison.Ordinal);
            if (end <= marker)
                return string.Empty;
            return message.Substring(marker, end - marker);
        }

        private static string ExtractDidChangeText(string message)
        {
            return ExtractDidOpenText(message);
        }

        private static string ExtractId(string message)
        {
            int index = message.IndexOf("\"id\":", StringComparison.Ordinal);
            if (index < 0)
                return "0";
            index += "\"id\":".Length;
            int end = index;
            while (end < message.Length && char.IsDigit(message[end]))
                end++;
            return end > index ? message.Substring(index, end - index) : "0";
        }

        private static void PublishDiagnostics(Stream output, string uri, string text)
        {
            var diagnostics = new List<string>();
            int line = 0;
            int column = 0;
            bool inString = false;
            for (int i = 0; i < text.Length; ++i)
            {
                char c = text[i];
                if (c == '\n')
                {
                    line++;
                    column = 0;
                    inString = false;
                    continue;
                }
                if (c == '"')
                {
                    inString = !inString;
                }
                if (!inString && c == '@')
                {
                    diagnostics.Add("{\"range\":{\"start\":{\"line\":" + line + ",\"character\":" + column + "},\"end\":{\"line\":" + line + ",\"character\":" + (column + 1) + "}},\"severity\":1,\"source\":\"RTSL\",\"message\":\"unexpected character '@'\"}");
                }
                column++;
            }

            WriteJson(output, "{\"method\":\"textDocument/publishDiagnostics\",\"params\":{\"uri\":\"" + EscapeJson(uri) + "\",\"diagnostics\":[" + string.Join(",", diagnostics) + "]}}");
        }

        private static string BuildSemanticTokens(string text)
        {
            var tokens = new List<int>();
            int prevLine = 0;
            int prevChar = 0;
            int line = 0;
            int col = 0;

            for (int i = 0; i < text.Length;)
            {
                char c = text[i];
                if (c == '\n')
                {
                    line++;
                    col = 0;
                    i++;
                    continue;
                }
                if (char.IsWhiteSpace(c))
                {
                    col++;
                    i++;
                    continue;
                }

                if (IsIdentStart(c))
                {
                    int startLine = line;
                    int startCol = col;
                    int start = i;
                    while (i < text.Length && IsIdentContinue(text[i]))
                    {
                        i++;
                        col++;
                    }
                    var word = text.Substring(start, i - start);
                    var type = IsKeyword(word) ? 0 : 5;
                    AddSemanticToken(tokens, ref prevLine, ref prevChar, startLine, startCol, word.Length, type);
                    continue;
                }

                if (char.IsDigit(c))
                {
                    int startLine = line;
                    int startCol = col;
                    int start = i;
                    while (i < text.Length && char.IsDigit(text[i]))
                    {
                        i++;
                        col++;
                    }
                    AddSemanticToken(tokens, ref prevLine, ref prevChar, startLine, startCol, i - start, 2);
                    continue;
                }

                if (c == '/' && i + 1 < text.Length && text[i + 1] == '/')
                {
                    int startLine = line;
                    int startCol = col;
                    int start = i;
                    while (i < text.Length && text[i] != '\n')
                    {
                        i++;
                        col++;
                    }
                    AddSemanticToken(tokens, ref prevLine, ref prevChar, startLine, startCol, i - start, 3);
                    continue;
                }

                if (c == '"')
                {
                    int startLine = line;
                    int startCol = col;
                    int start = i;
                    i++;
                    col++;
                    while (i < text.Length && text[i] != '"')
                    {
                        if (text[i] == '\\' && i + 1 < text.Length)
                        {
                            i += 2;
                            col += 2;
                            continue;
                        }
                        if (text[i] == '\n')
                            break;
                        i++;
                        col++;
                    }
                    if (i < text.Length && text[i] == '"')
                    {
                        i++;
                        col++;
                    }
                    AddSemanticToken(tokens, ref prevLine, ref prevChar, startLine, startCol, i - start, 1);
                    continue;
                }

                AddSemanticToken(tokens, ref prevLine, ref prevChar, line, col, 1, 4);
                i++;
                col++;
            }

            return "[" + string.Join(",", tokens) + "]";
        }

        private static void AddSemanticToken(List<int> tokens, ref int prevLine, ref int prevChar, int line, int character, int length, int type)
        {
            tokens.Add(line - prevLine);
            tokens.Add(line == prevLine ? character - prevChar : character);
            tokens.Add(length);
            tokens.Add(type);
            tokens.Add(0);
            prevLine = line;
            prevChar = character;
        }

        private static bool IsIdentStart(char c) => char.IsLetter(c) || c == '_';
        private static bool IsIdentContinue(char c) => char.IsLetterOrDigit(c) || c == '_';

        private static bool IsKeyword(string word)
        {
            switch (word)
            {
                case "import":
                case "export":
                case "namespace":
                case "struct":
                case "using":
                case "uniform":
                case "fn":
                case "const":
                case "auto":
                case "void":
                case "if":
                case "else":
                case "while":
                case "do":
                case "for":
                case "return":
                case "clip":
                case "smooth":
                case "flat":
                case "readonly":
                case "writeonly":
                case "true":
                case "false":
                case "inout":
                case "input":
                case "output":
                case "location":
                case "builtin":
                case "layout":
                case "std140":
                case "std430":
                case "scalar":
                    return true;
                default:
                    return false;
            }
        }

        private static string EscapeJson(string text)
        {
            return text.Replace("\\", "\\\\").Replace("\"", "\\\"");
        }
    }
}
