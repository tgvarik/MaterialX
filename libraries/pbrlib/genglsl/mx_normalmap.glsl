void mx_normalmap(vec3 value, int tangent_space, vec3 N, vec3 T, out vec3 result)
{
    // Tangent space
    if (tangent_space == 1)
    {
        vec3 v = value * 2.0 - 1.0;
        vec3 B = normalize(cross(N, T));
        result = normalize(T * v.x + B * v.y + N * v.z);
    }
    // Object space
    else
    {
        vec3 n = value * 2.0 - 1.0;
        result = normalize(n);
    }
}
