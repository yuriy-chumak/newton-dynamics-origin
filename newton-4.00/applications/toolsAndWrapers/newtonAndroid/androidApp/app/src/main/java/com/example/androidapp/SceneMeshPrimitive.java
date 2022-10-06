/* Copyright (c) <2003-2022> <Newton Game Dynamics>
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely
 */

package com.example.androidapp;

import android.opengl.GLES30;

import com.javaNewton.nMatrix;
import com.javaNewton.nMeshEffect;
import com.javaNewton.nShapeInstance;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

public class SceneMeshPrimitive extends SceneMesh
{
    SceneMeshPrimitive(nShapeInstance shapeInstance, RenderScene scene)
    {
        super();

        m_shader = scene.GetShaderCache().m_solidColor;

        // get vertex data from mesh and make a vertex buffer for rendering
        int vertexSizeInFloats = (3 + 3 + 2);
        nMeshEffect meshEffect = new nMeshEffect(shapeInstance);

        int vertexCount = meshEffect.GetVertextCount();
        float[] vertexData = new float[vertexCount * vertexSizeInFloats];
        meshEffect.GetVertexPosit(vertexData, 0, vertexSizeInFloats);
        meshEffect.GetVertexNormal(vertexData, 3, vertexSizeInFloats);
        meshEffect.GetVertexUV0(vertexData, 6, vertexSizeInFloats);

        int vertexSizeInBytes = vertexSizeInFloats * 4;
        ByteBuffer bb = ByteBuffer.allocateDirect(vertexCount * vertexSizeInBytes);
        bb.order(ByteOrder.nativeOrder());
        vertexBuffer = bb.asFloatBuffer();
        vertexBuffer.put(vertexData);
        vertexBuffer.position(0);

        // I can't get GLES30.glGenBuffers tow works, does not seem to do anything at all.
        int[] m_vertexBuffer = new int[1];
        int[] m_vertextArrayBuffer = new int[1];
        m_vertexBuffer[0] = 0;
        m_vertextArrayBuffer[0] = 0;
        GLES30.glGenBuffers(1, m_vertexBuffer, 0);
        GLES30.glGenVertexArrays(1, m_vertextArrayBuffer, 0);

        GLES30.glBindBuffer(GLES30.GL_ARRAY_BUFFER, m_vertexBuffer[0]);
        GLES30.glBufferData(GLES30.GL_ARRAY_BUFFER, vertexSizeInBytes, vertexBuffer, GLES30.GL_STATIC_DRAW);

        GLES30.glBindVertexArray(m_vertextArrayBuffer[0]);

        // get index data from mesh and make a vertex buffer for rendering
        meshEffect.MaterialBegin();

        int indexCount = 0;
        for (int index = meshEffect.GetFirstMaterial(); index != -1; index = meshEffect.GetNextMaterial(index))
        {
            indexCount += meshEffect.GetMaterialIndexCount(index);
        }
        short[] indexData = new short[indexCount];

        int offset = 0;
        for (int index = meshEffect.GetFirstMaterial(); index != -1; index = meshEffect.GetNextMaterial(index))
        {
            meshEffect.GetMaterialGetIndexStream(index, indexData, offset);
            int segIndexCount = meshEffect.GetMaterialIndexCount(index);

            SceneMeshSegment segment = new SceneMeshSegment(offset, segIndexCount);
            AddSegment(segment);

            offset += segIndexCount;
        }
        meshEffect.MaterialEnd();

        ByteBuffer dlb = ByteBuffer.allocateDirect(indexCount * 2);
        dlb.order(ByteOrder.nativeOrder());
        drawListBuffer = dlb.asShortBuffer();
        drawListBuffer.put(indexData);
        drawListBuffer.position(0);
    }

    @Override
    public void Render (nMatrix matrix)
    {
        // Add program to OpenGL environment
        GLES30.glUseProgram(m_shader);

        GLES30.glUseProgram(0);
    }
}