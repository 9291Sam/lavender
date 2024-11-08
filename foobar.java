public class Noise {
    private final int seed;
    private final double frequency;
    private final String noiseType;
    private final int octaves;
    private final double lacunarity;
    private final double gain;
    private final double weightedStrength;
    private final double pingPongStrength;
    private final boolean fractalBounding;
    private final double cellularJitter;
    private final double domainWarp;

    public Noise(
        int seed,
        double frequency,
        String noiseType,
        int rotations,
        int octaves,
        double lacunarity,
        double gain,
        double weightedStrength,
        double pingPongStrength,
        boolean fractalBounding,
        double cellularJitter,
        double domainWarp
    );
    
    /* thousands of lines of code */
}

// Builder for Noise Class
public class NoiseBuilder
{
    public int seed = 13373313371337;
    public double frequency = 0.01;
    public String noiseType = "Perlin";
    public int octaves = 1;
    public double lacunarity = 1.0;
    public double gain = 1.0;
    public double weightedStrength = 1.0;
    public double pingPongStrength = 1.0;
    public boolean fractalBounding = true;
    public double cellularJitter = 0.0;
    public double domainWarp = 1.0;

    public NoiseBuilder() {}

    public Noise build() {
        return new Noise(
            seed,
            frequency,
            noiseType,
            octaves,
            lacunarity,
            gain,
            weightedStrength,
            pingPongStrength,
            fractalBounding,
            cellularJitter,
            domainWarp
        );
    }
}

public class BuilderPatternExample {
    public static void main(String[] args) {
        NoiseBuilder builder = new NoiseBuilder();
        
        builder.seed = 42;
        builder.frequency = 0.02;
        builder.noiseType = "Simplex";
        builder.octaves = 5;
        builder.lacunarity = 2.5;
        builder.gain = 0.6;
        builder.weightedStrength = 1.2;
        builder.pingPongStrength = 1.5;
        builder.fractalBounding = false;
        builder.cellularJitter = 0.4;
        builder.domainWarp = 0.8;

        Noise noise = builder.build();
    }
}
