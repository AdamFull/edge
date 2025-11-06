from fastapi import FastAPI, HTTPException, Response
from fastapi.responses import StreamingResponse, JSONResponse
from pydantic import BaseModel
from typing import Optional, List, Literal
from pathlib import Path
import os
import time
import zipfile
import io
import json

app = FastAPI(title="File Server")

ROOT_DIR = Path(".")
CHUNK_SIZE = 65536

class EntryInfo(BaseModel):
    """Information about a file or directory"""
    path: str
    type: Literal["file", "directory", "not_found"]
    size: int = 0
    mtime: int = 0
    
class FilesystemEntry(BaseModel):
    """Entry in filesystem tree"""
    path: str
    is_directory: bool
    size: int
    mtime: int


class FilesystemTree(BaseModel):
    """Complete filesystem tree"""
    entries: List[FilesystemEntry]
    
def resolve_path(request_path: str) -> Path:
    """Resolve and validate path relative to ROOT_DIR"""
    # Remove leading slash and resolve
    clean_path = request_path.lstrip("/")
    full_path = (ROOT_DIR / clean_path).resolve()
    
    # Security check: ensure path is within ROOT_DIR
    try:
        full_path.relative_to(ROOT_DIR.resolve())
    except ValueError:
        raise HTTPException(status_code=403, detail="Access denied: path outside root directory")
    
    return full_path


def get_mtime(path: Path) -> int:
    """Get modification time as Unix timestamp"""
    try:
        return int(path.stat().st_mtime)
    except:
        return 0


@app.get("/", response_model=dict)
async def root():
    """API information"""
    return {
        "name": "Remote File System Server",
        "version": "1.0",
        "root_directory": str(ROOT_DIR.resolve()),
        "endpoints": {
            "entry_info": "GET /api/entry_info?path=<path>",
            "download": "GET /api/download?path=<path>",
            "filesystem_tree": "GET /api/filesystem_tree?path=<path>&recursive=<bool>"
        }
    }


@app.get("/api/entry_info", response_model=EntryInfo)
async def entry_info(path: str = ""):
    """
    Get information about a file or directory
    
    Args:
        path: Relative path from root directory
    
    Returns:
        EntryInfo with type, size, and modification time
    """
    try:
        full_path = resolve_path(path)
        
        if not full_path.exists():
            return EntryInfo(
                path=path,
                type="not_found",
                size=0,
                mtime=0
            )
        
        if full_path.is_file():
            return EntryInfo(
                path=path,
                type="file",
                size=full_path.stat().st_size,
                mtime=get_mtime(full_path)
            )
        elif full_path.is_dir():
            return EntryInfo(
                path=path,
                type="directory",
                size=0,
                mtime=get_mtime(full_path)
            )
        else:
            return EntryInfo(
                path=path,
                type="not_found",
                size=0,
                mtime=0
            )
    
    except HTTPException:
        raise
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))


@app.get("/api/download")
async def download(path: str = "", compress: bool = False):
    """
    Download a file or directory
    
    Args:
        path: Relative path from root directory
        compress: If True and path is a directory, compress as ZIP
    
    Returns:
        Streaming file or ZIP archive
    """
    try:
        full_path = resolve_path(path)
        
        if not full_path.exists():
            raise HTTPException(status_code=404, detail="File or directory not found")
        
        # Download single file
        if full_path.is_file():
            def file_iterator():
                with open(full_path, "rb") as f:
                    while chunk := f.read(CHUNK_SIZE):
                        yield chunk
            
            return StreamingResponse(
                file_iterator(),
                media_type="application/octet-stream",
                headers={
                    "Content-Disposition": f'attachment; filename="{full_path.name}"',
                    "Content-Length": str(full_path.stat().st_size)
                }
            )
        
        # Download directory as ZIP
        elif full_path.is_dir():
            if not compress:
                raise HTTPException(
                    status_code=400, 
                    detail="Directory download requires compress=true parameter"
                )
            
            def zip_iterator():
                zip_buffer = io.BytesIO()
                
                with zipfile.ZipFile(zip_buffer, 'w', zipfile.ZIP_DEFLATED) as zip_file:
                    for item in full_path.rglob("*"):
                        if item.is_file():
                            arcname = item.relative_to(full_path)
                            zip_file.write(item, arcname)
                
                zip_buffer.seek(0)
                return zip_buffer.read()
            
            zip_data = zip_iterator()
            
            return Response(
                content=zip_data,
                media_type="application/zip",
                headers={
                    "Content-Disposition": f'attachment; filename="{full_path.name}.zip"'
                }
            )
        
        else:
            raise HTTPException(status_code=400, detail="Invalid path type")
    
    except HTTPException:
        raise
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))


@app.get("/api/filesystem_tree", response_model=FilesystemTree)
async def filesystem_tree(path: str = "", recursive: bool = False):
    """
    Get filesystem tree
    
    Args:
        path: Relative path from root directory (default: root)
        recursive: If True, include all subdirectories
    
    Returns:
        FilesystemTree with all entries
    """
    try:
        full_path = resolve_path(path)
        
        if not full_path.exists():
            raise HTTPException(status_code=404, detail="Directory not found")
        
        if not full_path.is_dir():
            raise HTTPException(status_code=400, detail="Path is not a directory")
        
        entries = []
        
        if recursive:
            for item in full_path.rglob("*"):
                try:
                    rel_path = item.relative_to(ROOT_DIR)
                    entries.append(FilesystemEntry(
                        path=str(rel_path).replace("\\", "/"),
                        is_directory=item.is_dir(),
                        size=item.stat().st_size if item.is_file() else 0,
                        mtime=get_mtime(item)
                    ))
                except:
                    continue
        else:
            for item in full_path.iterdir():
                try:
                    rel_path = item.relative_to(ROOT_DIR)
                    entries.append(FilesystemEntry(
                        path=str(rel_path).replace("\\", "/"),
                        is_directory=item.is_dir(),
                        size=item.stat().st_size if item.is_file() else 0,
                        mtime=get_mtime(item)
                    ))
                except:
                    continue
        
        return FilesystemTree(entries=entries)
    
    except HTTPException:
        raise
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))
    
if __name__ == "__main__":
    import argparse
    import uvicorn
    
    parser = argparse.ArgumentParser(description="Remote File System Server")
    parser.add_argument("directory", help="Root directory to serve")
    parser.add_argument("--port", type=int, default=8080, help="Port to listen on (default: 8080)")
    parser.add_argument("--host", default="0.0.0.0", help="Host to bind to (default: 0.0.0.0)")
    
    args = parser.parse_args()
    
    ROOT_DIR = Path(args.directory).resolve()
    
    if not ROOT_DIR.exists():
        print(f"Error: Directory does not exist: {ROOT_DIR}")
        exit(1)
    
    if not ROOT_DIR.is_dir():
        print(f"Error: Path is not a directory: {ROOT_DIR}")
        exit(1)
    
    print(f"Starting Remote File System Server")
    print(f"Serving directory: {ROOT_DIR}")
    print(f"Listening on: {args.host}:{args.port}")
    print(f"\nAPI Documentation: http://{args.host}:{args.port}/docs")
    
    uvicorn.run(app, host=args.host, port=args.port)