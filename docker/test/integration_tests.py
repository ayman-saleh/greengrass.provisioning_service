#!/usr/bin/env python3
"""
Integration tests for Greengrass Provisioning Service
"""

import pytest
import subprocess
import json
import os
import time
import sqlite3
from pathlib import Path
import requests
import shutil


class TestGreengrassProvisioning:
    """Integration tests for the Greengrass provisioning service"""
    
    @classmethod
    def setup_class(cls):
        """Set up test environment"""
        cls.database_path = "/opt/greengrass/config/devices.db"
        cls.greengrass_path = "/greengrass/v2"
        cls.status_file = "/test-results/provisioning.status"
        cls.binary_path = "/usr/local/bin/greengrass_provisioning_service"
        
        # Clean up any previous test runs
        if os.path.exists(cls.greengrass_path):
            shutil.rmtree(cls.greengrass_path)
        os.makedirs(cls.greengrass_path, exist_ok=True)
        
        if os.path.exists(cls.status_file):
            os.remove(cls.status_file)
    
    def read_status_file(self):
        """Read and parse the status file"""
        if not os.path.exists(self.status_file):
            return None
        
        with open(self.status_file, 'r') as f:
            return json.load(f)
    
    def wait_for_status(self, expected_status, timeout=30):
        """Wait for a specific status in the status file"""
        start_time = time.time()
        
        while time.time() - start_time < timeout:
            status = self.read_status_file()
            if status and status.get('status') == expected_status:
                return True
            
            if status and status.get('status') == 'ERROR':
                pytest.fail(f"Error status detected: {status.get('error_details', 'Unknown error')}")
            
            time.sleep(0.5)
        
        return False
    
    def test_binary_exists(self):
        """Test that the provisioning binary exists and is executable"""
        assert os.path.exists(self.binary_path), f"Binary not found at {self.binary_path}"
        assert os.access(self.binary_path, os.X_OK), f"Binary at {self.binary_path} is not executable"
    
    def test_help_command(self):
        """Test that the help command works"""
        result = subprocess.run([self.binary_path, "--help"], 
                              capture_output=True, text=True)
        
        assert result.returncode in [0, 1], f"Help command failed with code {result.returncode}"
        assert "greengrass" in result.stdout.lower() or "greengrass" in result.stderr.lower()
    
    def test_database_connectivity(self):
        """Test that we can connect to the test database"""
        assert os.path.exists(self.database_path), f"Database not found at {self.database_path}"
        
        conn = sqlite3.connect(self.database_path)
        cursor = conn.cursor()
        
        # Check device_config table exists
        cursor.execute("SELECT COUNT(*) FROM device_config")
        count = cursor.fetchone()[0]
        assert count > 0, "No devices found in database"
        
        conn.close()
    
    def test_mock_services_connectivity(self):
        """Test connectivity to mock AWS services"""
        # Test mock AWS IoT
        try:
            response = requests.get("http://mock-aws-iot:1080/", timeout=5)
            assert response.status_code == 200
        except Exception as e:
            pytest.fail(f"Cannot connect to mock AWS IoT: {e}")
        
        # Test mock S3
        try:
            response = requests.get("http://mock-s3:9000/minio/health/live", timeout=5)
            assert response.status_code == 200
        except Exception as e:
            pytest.fail(f"Cannot connect to mock S3: {e}")
    
    def test_provisioning_full_cycle(self):
        """Test the full provisioning cycle"""
        # Clean up before test
        if os.path.exists(self.status_file):
            os.remove(self.status_file)
        
        # Start provisioning process
        cmd = [
            self.binary_path,
            "-d", self.database_path,
            "-g", self.greengrass_path,
            "-s", self.status_file,
            "-v"
        ]
        
        process = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        
        # Monitor status progression
        assert self.wait_for_status("CHECKING_PROVISIONING", timeout=10), \
            "Failed to reach CHECKING_PROVISIONING status"
        
        assert self.wait_for_status("CHECKING_CONNECTIVITY", timeout=20), \
            "Failed to reach CHECKING_CONNECTIVITY status"
        
        assert self.wait_for_status("READING_DATABASE", timeout=30), \
            "Failed to reach READING_DATABASE status"
        
        assert self.wait_for_status("GENERATING_CONFIG", timeout=40), \
            "Failed to reach GENERATING_CONFIG status"
        
        # Wait for completion
        assert self.wait_for_status("COMPLETED", timeout=120), \
            "Failed to complete provisioning"
        
        # Wait for process to finish
        stdout, stderr = process.communicate(timeout=10)
        
        assert process.returncode == 0, \
            f"Provisioning failed with code {process.returncode}\nSTDOUT: {stdout}\nSTDERR: {stderr}"
        
        # Verify files were created
        assert os.path.exists(f"{self.greengrass_path}/config/config.yaml"), \
            "Config file was not created"
        
        cert_files = list(Path(f"{self.greengrass_path}/certs").glob("*.pem"))
        assert len(cert_files) >= 2, \
            f"Expected at least 2 certificate files, found {len(cert_files)}"
        
        key_files = list(Path(f"{self.greengrass_path}/certs").glob("*.key"))
        assert len(key_files) >= 1, \
            f"Expected at least 1 key file, found {len(key_files)}"
    
    def test_idempotency(self):
        """Test that running provisioning twice is idempotent"""
        # Remove status file
        if os.path.exists(self.status_file):
            os.remove(self.status_file)
        
        # Run provisioning again
        cmd = [
            self.binary_path,
            "-d", self.database_path,
            "-g", self.greengrass_path,
            "-s", self.status_file,
            "-v"
        ]
        
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
        
        assert result.returncode == 0, \
            f"Second provisioning run failed with code {result.returncode}"
        
        # Check status
        status = self.read_status_file()
        assert status is not None, "Status file not created"
        assert status['status'] == "ALREADY_PROVISIONED", \
            f"Expected ALREADY_PROVISIONED status, got {status['status']}"
    
    def test_config_file_content(self):
        """Test that the generated config file has correct content"""
        config_path = f"{self.greengrass_path}/config/config.yaml"
        assert os.path.exists(config_path), "Config file does not exist"
        
        with open(config_path, 'r') as f:
            config_content = f.read()
        
        # Check for required fields
        assert "system:" in config_content, "Missing system section"
        assert "thingName:" in config_content, "Missing thingName"
        assert "services:" in config_content, "Missing services section"
        assert "aws.greengrass.Nucleus:" in config_content, "Missing Nucleus service"
        assert "iotDataEndpoint:" in config_content, "Missing IoT endpoint"
        assert "mock-aws-iot:1080" in config_content, "Incorrect IoT endpoint"
    
    def test_certificate_permissions(self):
        """Test that certificates have correct permissions"""
        key_files = list(Path(f"{self.greengrass_path}/certs").glob("*.key"))
        
        for key_file in key_files:
            # Check that private keys have restricted permissions
            stat_info = os.stat(key_file)
            mode = stat_info.st_mode & 0o777
            
            # Private keys should be readable only by owner (0o600 or 0o400)
            assert mode in [0o600, 0o400], \
                f"Private key {key_file} has incorrect permissions: {oct(mode)}"
    
    def test_status_file_format(self):
        """Test that the status file has the correct format"""
        status = self.read_status_file()
        assert status is not None, "Status file not found"
        
        # Check required fields
        required_fields = ['status', 'message', 'timestamp', 'progress_percentage']
        for field in required_fields:
            assert field in status, f"Missing required field: {field}"
        
        # Check types
        assert isinstance(status['status'], str), "Status should be a string"
        assert isinstance(status['message'], str), "Message should be a string"
        assert isinstance(status['timestamp'], str), "Timestamp should be a string"
        assert isinstance(status['progress_percentage'], int), "Progress should be an integer"
        
        # Check progress percentage range
        assert 0 <= status['progress_percentage'] <= 100, \
            f"Progress percentage out of range: {status['progress_percentage']}"
    
    def test_error_handling_bad_database(self):
        """Test error handling with non-existent database"""
        if os.path.exists(self.status_file):
            os.remove(self.status_file)
        
        cmd = [
            self.binary_path,
            "-d", "/non/existent/database.db",
            "-g", self.greengrass_path,
            "-s", self.status_file
        ]
        
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=10)
        
        # Should fail with non-zero exit code
        assert result.returncode != 0, "Should fail with bad database path"
    
    @classmethod
    def teardown_class(cls):
        """Clean up after all tests"""
        # Save test artifacts for inspection
        test_artifacts_dir = "/test-results/artifacts"
        os.makedirs(test_artifacts_dir, exist_ok=True)
        
        # Copy generated files
        if os.path.exists(f"{cls.greengrass_path}/config/config.yaml"):
            shutil.copy(f"{cls.greengrass_path}/config/config.yaml", 
                       f"{test_artifacts_dir}/config.yaml")
        
        # Create test report
        report = {
            "test_completed": time.strftime("%Y-%m-%d %H:%M:%S UTC", time.gmtime()),
            "artifacts_saved": os.listdir(test_artifacts_dir) if os.path.exists(test_artifacts_dir) else []
        }
        
        with open("/test-results/pytest-report.json", "w") as f:
            json.dump(report, f, indent=2)


if __name__ == "__main__":
    pytest.main([__file__, "-v", "--tb=short"]) 