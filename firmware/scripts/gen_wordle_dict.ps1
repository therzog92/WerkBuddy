# Generate wordle_dict.cpp from wordle-allowed.txt
# Produces a sorted char array + binary search, compiled once (not in header).

$words = Get-Content (Join-Path $PSScriptRoot "wordle-allowed.txt") |
         Where-Object { $_.Length -eq 5 } |
         ForEach-Object { $_.ToUpper() } |
         Sort-Object -Unique

# Add custom slang/culture words that may not be in the official list
$extras = @(
  "TWINK","HUNTY","POPOF","CAMPY","GLAMS","BOLLY","CUNTY","TWINX",
  "SLAYY","BODYS","HOTTY","CUBBY","DILFS","DERPY","GOOPY"
)
foreach ($w in $extras) {
  if ($words -notcontains $w) { $words += $w }
}
$words = $words | Sort-Object -Unique

$count = $words.Count
Write-Host "$count words total"

# Build the .cpp file
$sb = [System.Text.StringBuilder]::new()
[void]$sb.AppendLine('#include "games/wordle_dict.h"')
[void]$sb.AppendLine('#include <cstring>')
[void]$sb.AppendLine('')
[void]$sb.AppendLine('namespace wp { namespace games { namespace wordle {')
[void]$sb.AppendLine('')
[void]$sb.AppendLine("static constexpr int kDictCount = $count;")
[void]$sb.AppendLine('')
[void]$sb.AppendLine('/* Sorted, contiguous 5-char blocks (no NUL between words). */')
[void]$sb.AppendLine('static const char kDict[] =')

# Write 10 words per line as adjacent string literals
for ($i = 0; $i -lt $words.Count; $i += 10) {
  $end = [Math]::Min($i + 10, $words.Count)
  $line = "    "
  for ($j = $i; $j -lt $end; $j++) {
    $line += '"' + $words[$j] + '"'
    if ($j -lt $words.Count - 1) { $line += ' ' }
  }
  if ($end -ge $words.Count) { $line += ';' }
  [void]$sb.AppendLine($line)
}

[void]$sb.AppendLine('')
[void]$sb.AppendLine('bool dict_contains(const char * word) {')
[void]$sb.AppendLine('  if (!word) return false;')
[void]$sb.AppendLine('  char upper[6];')
[void]$sb.AppendLine('  for (int i = 0; i < 5; ++i) {')
[void]$sb.AppendLine('    char c = word[i];')
[void]$sb.AppendLine('    if (c >= ''a'' && c <= ''z'') c -= 32;')
[void]$sb.AppendLine('    upper[i] = c;')
[void]$sb.AppendLine('  }')
[void]$sb.AppendLine('  upper[5] = ''\0'';')
[void]$sb.AppendLine('  /* Binary search on sorted 5-char blocks */')
[void]$sb.AppendLine('  int lo = 0, hi = kDictCount - 1;')
[void]$sb.AppendLine('  while (lo <= hi) {')
[void]$sb.AppendLine('    int mid = (lo + hi) / 2;')
[void]$sb.AppendLine('    int cmp = std::strncmp(upper, kDict + mid * 5, 5);')
[void]$sb.AppendLine('    if (cmp == 0) return true;')
[void]$sb.AppendLine('    if (cmp < 0) hi = mid - 1; else lo = mid + 1;')
[void]$sb.AppendLine('  }')
[void]$sb.AppendLine('  return false;')
[void]$sb.AppendLine('}')
[void]$sb.AppendLine('')
[void]$sb.AppendLine('}}}  // namespace wp::games::wordle')

$cppPath = Join-Path $PSScriptRoot "..\src\games\wordle_dict.cpp"
[System.IO.File]::WriteAllText($cppPath, $sb.ToString(), [System.Text.Encoding]::UTF8)
Write-Host "Wrote $cppPath"

