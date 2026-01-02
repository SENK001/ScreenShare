# PowerShell脚本：将多个PNG文件转换为单个多分辨率的ICO文件
# 使用.NET的System.Drawing来处理图像

Add-Type -AssemblyName System.Drawing

$sourceDir = "$PSScriptRoot\..\res"
$outputFile = "$sourceDir\app.ico"

# PNG文件列表（按尺寸从小到大排序）
$pngFiles = @(
    "icon_16.png",
    "icon_20.png",
    "icon_24.png",
    "icon_32.png",
    "icon_40.png",
    "icon_48.png",
    "icon_64.png",
    "icon_256.png"
)

Write-Host "开始转换PNG到ICO..." -ForegroundColor Cyan

# 加载所有PNG图像
$images = @()
foreach ($pngFile in $pngFiles) {
    $pngPath = Join-Path $sourceDir $pngFile
    if (Test-Path $pngPath) {
        $img = [System.Drawing.Image]::FromFile($pngPath)
        $images += $img
        Write-Host "  已加载: $pngFile ($($img.Width)x$($img.Height))" -ForegroundColor Green
    } else {
        Write-Host "  警告: 未找到 $pngFile" -ForegroundColor Yellow
    }
}

if ($images.Count -eq 0) {
    Write-Host "错误: 没有找到任何PNG文件!" -ForegroundColor Red
    exit 1
}

# 创建ICO文件
try {
    $outputStream = [System.IO.File]::Create($outputFile)
    
    # ICO文件头
    # ICONDIR结构: Reserved(2字节) + Type(2字节) + Count(2字节)
    $writer = [System.IO.BinaryWriter]::new($outputStream)
    $writer.Write([UInt16]0)  # Reserved
    $writer.Write([UInt16]1)  # Type (1 = ICO)
    $writer.Write([UInt16]$images.Count)  # Count
    
    # 计算图像数据的起始偏移
    $imageDataOffset = 6 + ($images.Count * 16)  # Header + (ICONDIRENTRY * count)
    
    # 保存所有图像数据到内存流
    $imageDataStreams = @()
    foreach ($img in $images) {
        $ms = [System.IO.MemoryStream]::new()
        $img.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
        $imageDataStreams += $ms
    }
    
    # 写入ICONDIRENTRY结构
    $currentOffset = $imageDataOffset
    for ($i = 0; $i -lt $images.Count; $i++) {
        $img = $images[$i]
        $dataSize = $imageDataStreams[$i].Length
        
        # ICONDIRENTRY: Width(1) + Height(1) + ColorCount(1) + Reserved(1) + 
        #               Planes(2) + BitCount(2) + BytesInRes(4) + ImageOffset(4)
        $width = if ($img.Width -ge 256) { 0 } else { $img.Width }
        $height = if ($img.Height -ge 256) { 0 } else { $img.Height }
        
        $writer.Write([byte]$width)
        $writer.Write([byte]$height)
        $writer.Write([byte]0)  # ColorCount (0 for PNG)
        $writer.Write([byte]0)  # Reserved
        $writer.Write([UInt16]1)  # Planes
        $writer.Write([UInt16]32)  # BitCount (32-bit RGBA)
        $writer.Write([UInt32]$dataSize)
        $writer.Write([UInt32]$currentOffset)
        
        $currentOffset += $dataSize
    }
    
    # 写入所有图像数据
    foreach ($ms in $imageDataStreams) {
        $ms.Position = 0
        $ms.CopyTo($outputStream)
    }
    
    $writer.Close()
    $outputStream.Close()
    
    Write-Host "`n成功创建ICO文件: $outputFile" -ForegroundColor Green
    Write-Host "包含 $($images.Count) 个图标尺寸" -ForegroundColor Green
    
} catch {
    Write-Host "错误: $_" -ForegroundColor Red
    exit 1
} finally {
    # 清理资源
    foreach ($img in $images) {
        $img.Dispose()
    }
    foreach ($ms in $imageDataStreams) {
        $ms.Dispose()
    }
}

Write-Host "`n转换完成!" -ForegroundColor Cyan
