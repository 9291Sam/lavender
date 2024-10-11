#version 460

layout(location = 0) in flat u32 in_face_data;

layout(location = 0) out u32 out_face_data;

void main()
{
    out_face_data = in_face_data;  
}
