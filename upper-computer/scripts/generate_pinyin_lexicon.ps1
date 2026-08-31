param(
    [Parameter(Mandatory = $true)]
    [string]$SourceTable,
    [string]$OutputPath = (Join-Path $PSScriptRoot '..\data\input\pinyin_lexicon.tsv'),
    [string]$DomainPath = (Join-Path $PSScriptRoot '..\data\input\domain_pinyin.tsv'),
    [int]$MinimumWeight = 1000,
    [int]$MaximumCandidates = 8
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName Microsoft.VisualBasic

$source = (Resolve-Path -LiteralPath $SourceTable).Path
$output = [IO.Path]::GetFullPath($OutputPath)
$outputDirectory = Split-Path -Parent $output
[IO.Directory]::CreateDirectory($outputDirectory) | Out-Null

$groups = @{}
$reader = [IO.StreamReader]::new($source, [Text.Encoding]::UTF8)
try {
    while (($line = $reader.ReadLine()) -ne $null) {
        if ($line.Length -eq 0 -or $line[0] -eq '#') { continue }
        $fields = $line.Split("`t")
        if ($fields.Length -lt 3) { continue }
        $weight = 0L
        if (-not [long]::TryParse($fields[2], [ref]$weight) -or $weight -lt $MinimumWeight) { continue }
        $word = $fields[0].Trim()
        if ($word.Length -lt 1 -or $word.Length -gt 5) { continue }
        $pinyin = ($fields[1] -replace '[^A-Za-z]', '').ToLowerInvariant()
        if ($pinyin.Length -lt 1 -or $pinyin.Length -gt 24) { continue }
        $simplified = [Microsoft.VisualBasic.Strings]::StrConv(
            $word, [Microsoft.VisualBasic.VbStrConv]::SimplifiedChinese, 2052)
        if (-not $groups.ContainsKey($pinyin)) {
            $groups[$pinyin] = [Collections.Generic.List[object]]::new()
        }
        $groups[$pinyin].Add([pscustomobject]@{ Word = $simplified; Weight = $weight })
    }
}
finally {
    $reader.Dispose()
}

$domain = [ordered]@{}
foreach ($line in [IO.File]::ReadAllLines([IO.Path]::GetFullPath($DomainPath), [Text.Encoding]::UTF8)) {
    if ([string]::IsNullOrWhiteSpace($line) -or $line.StartsWith('#')) { continue }
    $fields = $line.Split("`t", [StringSplitOptions]::RemoveEmptyEntries)
    if ($fields.Length -ge 2) { $domain[$fields[0]] = @($fields[1..($fields.Length - 1)]) }
}

$utf8NoBom = [Text.UTF8Encoding]::new($false)
$writer = [IO.StreamWriter]::new($output, $false, $utf8NoBom)
try {
    $writer.WriteLine('# PressureOS compact Simplified-Chinese pinyin lexicon')
    $writer.WriteLine('# Generated from Rime Luna Pinyin; see THIRD_PARTY_NOTICES.md')
    foreach ($key in ($groups.Keys | Sort-Object)) {
        $words = [Collections.Generic.List[string]]::new()
        if ($domain.Contains($key)) {
            foreach ($word in $domain[$key]) { if (-not $words.Contains($word)) { $words.Add($word) } }
        }
        foreach ($item in ($groups[$key] | Sort-Object Weight -Descending)) {
            if (-not $words.Contains($item.Word)) { $words.Add($item.Word) }
            if ($words.Count -ge $MaximumCandidates) { break }
        }
        if ($words.Count -gt 0) { $writer.WriteLine($key + "`t" + ($words -join "`t")) }
    }
    foreach ($key in $domain.Keys) {
        if (-not $groups.ContainsKey($key)) { $writer.WriteLine($key + "`t" + ($domain[$key] -join "`t")) }
    }
}
finally {
    $writer.Dispose()
}

Write-Host "Generated $output with $($groups.Count) base pinyin keys."
