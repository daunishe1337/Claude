package com.neorig.pcbuilder.render.gl

import android.opengl.GLES30
import com.neorig.pcbuilder.render.scene.MeshData
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.nio.FloatBuffer
import java.nio.IntBuffer
import java.nio.ShortBuffer

/**
 * Меш в видеопамяти: VAO + буферы позиций, нормалей, цветов и индексов.
 * Создаётся и удаляется только в GL-потоке.
 */
class GpuMesh(data: MeshData) {

    private val vao = IntArray(1)
    private val buffers = IntArray(4)
    val indexCount: Int = data.indices.size
    val hasColors: Boolean = data.colors != null
    private val indexType: Int

    init {
        GLES30.glGenVertexArrays(1, vao, 0)
        GLES30.glGenBuffers(4, buffers, 0)
        GLES30.glBindVertexArray(vao[0])

        upload(buffers[0], floatBuffer(data.positions), location = 0, size = 3)
        upload(buffers[1], floatBuffer(data.normals), location = 1, size = 3)
        if (data.colors != null) {
            upload(buffers[2], floatBuffer(data.colors), location = 2, size = 4)
        } else {
            GLES30.glDisableVertexAttribArray(2)
        }

        GLES30.glBindBuffer(GLES30.GL_ELEMENT_ARRAY_BUFFER, buffers[3])
        if (data.vertexCount <= 65_535) {
            indexType = GLES30.GL_UNSIGNED_SHORT
            val shorts = ShortArray(data.indices.size) { data.indices[it].toShort() }
            GLES30.glBufferData(GLES30.GL_ELEMENT_ARRAY_BUFFER, shorts.size * 2, shortBuffer(shorts), GLES30.GL_STATIC_DRAW)
        } else {
            indexType = GLES30.GL_UNSIGNED_INT
            GLES30.glBufferData(GLES30.GL_ELEMENT_ARRAY_BUFFER, data.indices.size * 4, intBuffer(data.indices), GLES30.GL_STATIC_DRAW)
        }
        GLES30.glBindVertexArray(0)
    }

    fun draw() {
        GLES30.glBindVertexArray(vao[0])
        if (!hasColors) {
            // Нет массива цветов — используем постоянный белый цвет атрибута
            GLES30.glVertexAttrib4f(2, 1f, 1f, 1f, 1f)
        }
        GLES30.glDrawElements(GLES30.GL_TRIANGLES, indexCount, indexType, 0)
        GLES30.glBindVertexArray(0)
    }

    fun release() {
        GLES30.glDeleteBuffers(4, buffers, 0)
        GLES30.glDeleteVertexArrays(1, vao, 0)
    }

    private fun upload(buffer: Int, data: FloatBuffer, location: Int, size: Int) {
        GLES30.glBindBuffer(GLES30.GL_ARRAY_BUFFER, buffer)
        GLES30.glBufferData(GLES30.GL_ARRAY_BUFFER, data.capacity() * 4, data, GLES30.GL_STATIC_DRAW)
        GLES30.glEnableVertexAttribArray(location)
        GLES30.glVertexAttribPointer(location, size, GLES30.GL_FLOAT, false, 0, 0)
    }

    private companion object {
        fun floatBuffer(values: FloatArray): FloatBuffer =
            ByteBuffer.allocateDirect(values.size * 4).order(ByteOrder.nativeOrder()).asFloatBuffer().apply {
                put(values)
                position(0)
            }

        fun shortBuffer(values: ShortArray): ShortBuffer =
            ByteBuffer.allocateDirect(values.size * 2).order(ByteOrder.nativeOrder()).asShortBuffer().apply {
                put(values)
                position(0)
            }

        fun intBuffer(values: IntArray): IntBuffer =
            ByteBuffer.allocateDirect(values.size * 4).order(ByteOrder.nativeOrder()).asIntBuffer().apply {
                put(values)
                position(0)
            }
    }
}
