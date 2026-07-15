CREATE TABLE IF NOT EXISTS locations (
    id BIGSERIAL PRIMARY KEY,
    name TEXT NOT NULL,
    site_number TEXT NOT NULL,
    distance_from_source_km NUMERIC(12, 3) NOT NULL,
    description TEXT NOT NULL DEFAULT '',
    latitude DOUBLE PRECISION NOT NULL,
    longitude DOUBLE PRECISION NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE IF NOT EXISTS metals (
    id BIGSERIAL PRIMARY KEY,
    name TEXT NOT NULL,
    symbol TEXT NOT NULL,
    unit TEXT
);

CREATE TABLE IF NOT EXISTS samples (
    id BIGSERIAL PRIMARY KEY,
    metal_id BIGINT NOT NULL REFERENCES metals(id) ON DELETE CASCADE,
    location_id BIGINT NOT NULL REFERENCES locations(id) ON DELETE CASCADE,
    position TEXT NOT NULL,
    repetition INTEGER NOT NULL DEFAULT 1,
    value NUMERIC(14, 6),
    sampling_date DATE NOT NULL,
    analytics_number TEXT,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE IF NOT EXISTS concentration_jobs (
    id UUID PRIMARY KEY,
    reference_location_id BIGINT NOT NULL REFERENCES locations(id),
    metal_id BIGINT NOT NULL REFERENCES metals(id),
    sample_year INTEGER NOT NULL,
    grid_step_km NUMERIC(8, 3) NOT NULL DEFAULT 0.7,
    area_size_km NUMERIC(8, 3) NOT NULL DEFAULT 200.0,
    status TEXT NOT NULL DEFAULT 'queued',
    error_message TEXT,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    completed_at TIMESTAMPTZ
);

CREATE TABLE IF NOT EXISTS concentration_grid_points (
    job_id UUID NOT NULL REFERENCES concentration_jobs(id) ON DELETE CASCADE,
    latitude DOUBLE PRECISION NOT NULL,
    longitude DOUBLE PRECISION NOT NULL,
    concentration DOUBLE PRECISION NOT NULL,
    PRIMARY KEY (job_id, latitude, longitude)
);

CREATE INDEX IF NOT EXISTS idx_samples_location ON samples(location_id);
CREATE INDEX IF NOT EXISTS idx_samples_metal_year ON samples(metal_id, sampling_date);
CREATE INDEX IF NOT EXISTS idx_grid_points_job ON concentration_grid_points(job_id);

